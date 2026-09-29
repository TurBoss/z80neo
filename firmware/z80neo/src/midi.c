// midi.c — firmware-side Standard MIDI File parser/streamer
// z80neo — TurBoss 2026
//
// Parses an SMF from the SD card (FatFS) and merges its tracks into a stream
// of compact grouped records for the Z80 MIDI player (see include/midi.h).
// The Pico does the parsing so the 128 kHz Z80 only has to wait and emit; the
// Z80-side C parser could not keep up.  Kept read-only, one file at a time.

#include "midi.h"

#include <stdio.h>
#include <string.h>

#include <pico/stdlib.h>

#include "ff.h"
#include "fileio.h"
#include "memory.h"

#define MIDI_MAX_TRACKS 16
#define MIDI_CHUNK      512

// The Z80 replay loop costs a roughly fixed amount per record (instruction
// fetch + port access with WAIT) that is not part of the musical timeline.
// Rather than model that cost with fixed constants, the firmware schedules each
// record against an absolute playback clock (the RP2350 microsecond timer) and
// recomputes the delay from that clock on every record.  The Z80's replay cost
// is therefore measured (it shows up as elapsed time before the next record is
// requested) and absorbed, so a long song cannot drift no matter how the Z80's
// wait loop is calibrated.  These two values are an optional extra trim in
// tenths of a millisecond (subtracted from the delay, so raising them plays
// faster); the defaults are 0 and normally need no tuning.  Overridable from
// Z80NEO.INI as MIDI_GROUP_X10 / MIDI_BYTE_X10.
uint32_t midi_credit_group_x10 = 0u;    // extra trim per record (tenths of ms)
uint32_t midi_credit_byte_x10  = 0u;    // extra trim per forwarded byte

// Largest number of raw MIDI bytes in one record.  A tick with more messages
// is split across several records at the same tick (the extras get delay 0).
// Must stay below 0xFF, which is the end-of-song sentinel.
#define MIDI_GROUP_MAX 254u

typedef struct {
    uint32_t pos;                 // absolute file offset of the next byte
    uint32_t end;                 // absolute end of the track data
    uint32_t buf;                 // absolute offset of data[0]
    uint16_t blen;                // valid bytes in data[]
    uint8_t  data[MIDI_CHUNK];
    uint32_t tick;                // absolute tick of the pending event
    uint8_t  run;                 // running status
    uint8_t  pend;                // 0 none, 1 channel, 2 tempo, 3 end-of-track
    uint8_t  st, d1, d2, dlen;
    uint8_t  tempo[3];
    uint8_t  done;
} mtrack_t;

// The whole-file image is far larger than the system heap: the RP2350 bss
// already holds the 20 x 16 KB RAM banks, leaving only ~120 KB of heap.  A
// 100 KB+ file therefore only just fits a fresh heap, and malloc/free of a
// differently-sized buffer per song leaves non-adjacent free holes, so the
// next large request fails and pico_malloc panics "Out of memory".  Keep the
// image in PSRAM instead, above the PSRAM banks, and leave the system heap for
// the tiny per-frame OLED buffer.  With no PSRAM the parser streams from SD
// through the per-track buffer (m_img == NULL), which always works.
#define MIDI_IMG_MAX (2u * 1024u * 1024u)

static FIL      m_fil;
static uint8_t *m_img;          // whole file (PSRAM), or NULL to stream from SD
static uint32_t m_size;
static mtrack_t m_trk[MIDI_MAX_TRACKS];
static uint8_t  m_ntrk;
static bool     m_open;
static bool     m_ppqn;         // tempo meta events retime ticks
static uint32_t m_tempo_us;     // microseconds per quarter note
static uint32_t m_us_num;       // microseconds-per-tick numerator
static uint32_t m_us_den;       // microseconds-per-tick denominator (never 0)
static uint32_t m_global_tick;  // tick the pending events are scheduled from
static uint64_t m_base_us;      // absolute time (us) that m_global_tick is due
static uint64_t m_tick_rem;     // sub-us numerator carried between ticks
static bool     m_started;      // m_base_us has been anchored to the timer
static uint8_t  m_grp[3 + MIDI_GROUP_MAX];  // delay lo, delay hi, n, raw MIDI bytes
static uint16_t m_grp_len;      // bytes valid in m_grp
static uint16_t m_grp_pos;      // next byte to hand to the Z80

static uint32_t be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static uint16_t be16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

// Read one byte from a track, refilling the per-track buffer from SD as needed.
static uint8_t trd8(mtrack_t *t) {
    if (t->pos >= t->end) return 0;
    if (m_img)
        return m_img[t->pos++];
    if (t->pos < t->buf || t->pos >= t->buf + t->blen) {
        uint32_t want = t->end - t->pos;
        UINT br = 0;
        if (want > MIDI_CHUNK) want = MIDI_CHUNK;
        if (f_lseek(&m_fil, t->pos) != FR_OK) return 0;
        if (f_read(&m_fil, t->data, want, &br) != FR_OK || br == 0) {
            t->pos = t->end;
            return 0;
        }
        t->buf = t->pos;
        t->blen = (uint16_t)br;
    }
    return t->data[t->pos++ - t->buf];
}

static uint16_t trd16(mtrack_t *t) {
    uint16_t a = trd8(t);
    return (uint16_t)((a << 8) | trd8(t));
}

static uint32_t trd32(mtrack_t *t) {
    uint32_t v = 0;
    for (uint8_t i = 0; i < 4; i++)
        v = (v << 8) | trd8(t);
    return v;
}

static uint32_t tskip_val(mtrack_t *t) {
    uint32_t v = 0;
    uint8_t c;
    do {
        c = trd8(t);
        v = (v << 7) | (c & 0x7F);
    } while (c & 0x80);
    return v;
}

static void tskip(mtrack_t *t, uint32_t n) {
    t->pos += n;
    if (t->pos > t->end) t->pos = t->end;
}

// Load the next schedulable event (channel, tempo, or EOT) into the track.
static void m_read_event(mtrack_t *t) {
    uint32_t delta;
    uint8_t b, st, d1, d2, dl;

    t->pend = 0;

    for (;;) {
        if (t->done || t->pos >= t->end) {
            t->done = 1;
            t->pend = 0;
            return;
        }

        delta = tskip_val(t);
        b = trd8(t);

        if (b == 0xFF) {                       // meta event
            uint8_t type = trd8(t);
            uint32_t len = tskip_val(t);
            if (type == 0x2F) {                // end of track
                tskip(t, len);
                t->tick += delta;
                t->done = 1;
                t->pend = 3;
                return;
            }
            if (type == 0x51 && len == 3) {    // set tempo
                t->tempo[0] = trd8(t);
                t->tempo[1] = trd8(t);
                t->tempo[2] = trd8(t);
                t->tick += delta;
                t->pend = 2;
                return;
            }
            tskip(t, len);
            t->tick += delta;
            continue;
        }

        if (b == 0xF0 || b == 0xF7) {          // sysex: skip
            uint32_t len = tskip_val(t);
            tskip(t, len);
            t->tick += delta;
            continue;
        }

        dl = 0;
        d1 = 0;
        d2 = 0;
        if (b < 0x80) {
            st = t->run;                       // running status
            if (st < 0x80) { t->tick += delta; continue; }
            d1 = b;
            dl = (st >= 0xC0 && st <= 0xDF) ? 1 : 2;
            if (dl == 2)
                d2 = trd8(t);
        } else {
            st = b;
            if (st < 0xF0)
                t->run = st;
            if (st >= 0x80 && st <= 0xEF) {
                dl = (st >= 0xC0 && st <= 0xDF) ? 1 : 2;
                d1 = trd8(t);
                if (dl == 2)
                    d2 = trd8(t);
            } else if (st == 0xF1 || st == 0xF3) {
                d1 = trd8(t); dl = 1;
            } else if (st == 0xF2) {
                d1 = trd8(t); d2 = trd8(t); dl = 2;
            }
        }

        t->tick += delta;

        if (st >= 0x80 && st <= 0xEF) {
            t->pend = 1;
            t->st = st;
            t->d1 = d1;
            t->d2 = d2;
            t->dlen = (uint8_t)(dl + 1);       // status + data bytes
            return;
        }
        // non-channel system message: keep scanning
    }
}

// Find the pending event with the lowest absolute tick.  Returns -1 if none.
static int8_t m_best_track(uint32_t *tick) {
    int8_t best = -1;
    uint32_t bt = 0;

    for (uint8_t i = 0; i < m_ntrk; i++) {
        if (m_trk[i].pend && (best < 0 || m_trk[i].tick < bt)) {
            best = (int8_t)i;
            bt = m_trk[i].tick;
        }
    }
    *tick = bt;
    return best;
}

// Exact microseconds from tick `from` to tick `to`, carrying the sub-us
// remainder in *rem_out.  m_us_num/m_us_den express us-per-tick as a rational
// so a tempo that does not divide evenly cannot drift over a long song.  The
// caller commits *rem_out only once the tick is consumed, so a record that is
// re-scheduled (see the long-gap split below) does not advance it twice.
static uint64_t m_delta_us(uint32_t from, uint32_t to, uint64_t *rem_out) {
    uint64_t numer = (uint64_t)(to - from) * (uint64_t)m_us_num + m_tick_rem;
    *rem_out = numer % m_us_den;
    return numer / m_us_den;
}

// ---------------------------------------------------------------------------
// Console full-screen MIDI monitor
//
// The firmware already sees every channel message as it builds a record, so it
// keeps a 16-channel table (note, velocity, program, volume, pan) for free and
// paints a full-screen monitor on the console with VT102 cursor positioning;
// the Z80 neither parses nor draws for it.  The screen is cleared once at the
// start of a song and every row is addressed absolutely, so nothing scrolls.
//
// The header shows the file name, elapsed / total time (total is pre-scanned
// when the whole file is in PSRAM), tempo and track count.  Each channel row
// shows the current note, velocity bar, General MIDI instrument, volume and
// pan.  A row is rewritten only when its state actually changes, at most
// MIDI_PANEL_ROWS_PER_TICK rows per MIDI_PANEL_INTERVAL_US, and the header is
// refreshed only when the elapsed second or tempo changes, keeping console
// bandwidth far below the 115200 baud drain.
//
// Events are applied when the record carrying them is *due*, not when it is
// built, so the monitor never runs ahead of the music (a song's first note can
// sit behind a long initial delay).
// ---------------------------------------------------------------------------
#define MIDI_PANEL_CH             16
#define MIDI_PANEL_EV_MAX         128
#define MIDI_PANEL_INTERVAL_US    33000u
#define MIDI_PANEL_ROWS_PER_TICK  4
#define MIDI_PANEL_SONG_MAX       64

#define PN_ROW_TITLE 1
#define PN_ROW_SONG  2
#define PN_ROW_INFO  3
#define PN_ROW_COLS  4
#define PN_ROW_CH0   5                 // channel ch -> row PN_ROW_CH0 + ch

static bool     m_pnl_enabled = true;
static bool     m_pnl_want;                     // midi_open asked for a panel
static bool     m_pnl_on;                       // panel is on screen
static char     m_pnl_song[MIDI_PANEL_SONG_MAX];

static bool     m_pnl_active[MIDI_PANEL_CH];
static uint8_t  m_pnl_note[MIDI_PANEL_CH];
static uint8_t  m_pnl_vel[MIDI_PANEL_CH];
static uint8_t  m_pnl_prog[MIDI_PANEL_CH];
static bool     m_pnl_hasprog[MIDI_PANEL_CH];
static uint8_t  m_pnl_vol[MIDI_PANEL_CH];
static uint8_t  m_pnl_pan[MIDI_PANEL_CH];

static bool     m_pnl_sactive[MIDI_PANEL_CH];   // values currently shown
static uint8_t  m_pnl_snote[MIDI_PANEL_CH];
static uint8_t  m_pnl_svel[MIDI_PANEL_CH];
static uint8_t  m_pnl_sprog[MIDI_PANEL_CH];
static uint8_t  m_pnl_svol[MIDI_PANEL_CH];
static uint8_t  m_pnl_span[MIDI_PANEL_CH];

static uint16_t m_pnl_dirty;                    // bitmask of channels to repaint
static uint32_t m_pnl_last_us;
static uint16_t m_pnl_hdr_sec = 0xFFFF;         // last elapsed second drawn
static uint16_t m_pnl_hdr_bpm = 0xFFFF;         // last tempo drawn

static uint64_t m_pnl_total_us;                 // 0 = unknown
static uint64_t m_pnl_start_us;                 // playback start (first record)
static bool     m_pnl_time_started;

static bool     m_pnl_pend;                     // events collected, not yet due
static uint64_t m_pnl_due_us;
static uint8_t  m_pnl_ev_n;
static uint8_t  m_pnl_ev_st[MIDI_PANEL_EV_MAX];
static uint8_t  m_pnl_ev_d1[MIDI_PANEL_EV_MAX];
static uint8_t  m_pnl_ev_d2[MIDI_PANEL_EV_MAX];

// General MIDI program names (0-127); rendered in a 16-char column.
static const char *const m_pnl_gm[] = {
    "Acoustic Grand", "Bright Acoustic", "Electric Grand", "Honky-tonk",
    "Electric Piano 1", "Electric Piano 2", "Harpsichord", "Clavinet",
    "Celesta", "Glockenspiel", "Music Box", "Vibraphone", "Marimba",
    "Xylophone", "Tubular Bells", "Dulcimer", "Drawbar Organ",
    "Percussive Organ", "Rock Organ", "Church Organ", "Reed Organ",
    "Accordion", "Harmonica", "Tango Accordion", "Nylon Guitar",
    "Steel Guitar", "Jazz Guitar", "Clean Guitar", "Muted Guitar",
    "Overdriven Guitar", "Distortion Guitar", "Guitar Harmonics",
    "Acoustic Bass", "Finger Bass", "Pick Bass", "Fretless Bass",
    "Slap Bass 1", "Slap Bass 2", "Synth Bass 1", "Synth Bass 2", "Violin",
    "Viola", "Cello", "Contrabass", "Tremolo Strings", "Pizzicato Strings",
    "Orchestral Harp", "Timpani", "String Ensemble", "String Ensemble 2",
    "Synth Strings 1", "Synth Strings 2", "Choir Aahs", "Voice Oohs",
    "Synth Voice", "Orchestra Hit", "Trumpet", "Trombone", "Tuba",
    "Muted Trumpet", "French Horn", "Brass Section", "Synth Brass 1",
    "Synth Brass 2", "Soprano Sax", "Alto Sax", "Tenor Sax", "Baritone Sax",
    "Oboe", "English Horn", "Bassoon", "Clarinet", "Piccolo", "Flute",
    "Recorder", "Pan Flute", "Blown Bottle", "Shakuhachi", "Whistle",
    "Ocarina", "Lead (square)", "Lead (sawtooth)", "Lead (calliope)",
    "Lead (chiff)", "Lead (charang)", "Lead (voice)", "Lead (fifths)",
    "Lead (bass+lead)", "Pad (new age)", "Pad (warm)", "Pad (polysynth)",
    "Pad (choir)", "Pad (bowed)", "Pad (metallic)", "Pad (halo)",
    "Pad (sweep)", "FX (rain)", "FX (soundtrack)", "FX (crystal)",
    "FX (atmosphere)", "FX (brightness)", "FX (goblins)", "FX (echoes)",
    "FX (sci-fi)", "Sitar", "Banjo", "Shamisen", "Koto", "Kalimba",
    "Bagpipe", "Fiddle", "Shanai", "Tinkle Bell", "Agogo", "Steel Drums",
    "Woodblock", "Taiko Drum", "Melodic Tom", "Synth Drum", "Reverse Cymbal",
    "Guitar Fret Noise", "Breath Noise", "Seashore", "Bird Tweet",
    "Telephone Ring", "Helicopter", "Applause", "Gunshot"
};
_Static_assert(sizeof(m_pnl_gm) / sizeof(m_pnl_gm[0]) == 128,
               "GM program name table must hold 128 entries");

static void pnl_unum(uint16_t v) {
    char b[6];
    uint8_t n = 0;
    if (v == 0) { console_putc('0'); return; }
    while (v) { b[n++] = (char)('0' + v % 10); v /= 10; }
    while (n) console_putc((uint8_t)b[--n]);
}

static void pnl_time(uint64_t us, char *out) {
    uint32_t secs = (uint32_t)(us / 1000000ULL);
    uint32_t m = secs / 60u, s = secs % 60u;
    if (m > 99u) { m = 99u; s = 59u; }
    snprintf(out, 8, "%02u:%02u", (unsigned)m, (unsigned)s);
}

// Elapsed time since the song started; 0 before the first record is due (the
// start time can be in the future during a long opening gap, hence the guard).
static uint64_t pnl_elapsed_us(void) {
    uint64_t now = time_us_64();
    if (!m_pnl_start_us || now < m_pnl_start_us) return 0;
    return now - m_pnl_start_us;
}

// Move to column 1 of `row` (1-based); the monitor owns the whole screen.
static void pnl_goto(uint16_t row) {
    console_puts("\x1b[");
    pnl_unum(row);
    console_puts(";1H");
}

static void pnl_color(uint8_t ch) {
    console_putc(0x1B); console_putc('[');
    pnl_unum((uint16_t)(ch < 8 ? 30 + ch : 90 + (ch - 8)));
    console_putc('m');
}

// Effective program value, with 0xFF meaning "no program change seen yet".
static uint8_t pnl_eprog(uint8_t ch) {
    return m_pnl_hasprog[ch] ? m_pnl_prog[ch] : 0xFF;
}

// Render one channel row into `out` (no cursor escapes).  Returns its length.
static uint8_t pnl_row(char *out, size_t cap, uint8_t ch) {
    static const char *const nm[12] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    const char *inst = "---";
    char note[5], vel[4], level[11], vol[4], pan[4];
    uint8_t i, fill;
    int p;

    if (m_pnl_hasprog[ch])
        inst = m_pnl_gm[m_pnl_prog[ch] & 0x7F];

    if (m_pnl_active[ch]) {
        uint8_t n = m_pnl_note[ch];
        int oct = (int)(n / 12) - 1;
        uint8_t k = 0;
        note[k++] = nm[n % 12][0];
        if (nm[n % 12][1]) note[k++] = nm[n % 12][1];
        if (oct < 0) { note[k++] = '-'; note[k++] = (char)('0' - oct); }
        else         { note[k++] = (char)('0' + oct); }
        while (k < 4) note[k++] = ' ';
        note[k] = '\0';
        snprintf(vel, sizeof vel, "%3u", (unsigned)m_pnl_vel[ch]);
        fill = (uint8_t)(((uint16_t)m_pnl_vel[ch] * 10u) / 127u);
        if (fill == 0) fill = 1;                 // any note lights one cell
    } else {
        strcpy(note, "----");
        strcpy(vel, "---");
        fill = 0;
    }
    for (i = 0; i < 10; i++)
        level[i] = (i < fill) ? '#' : ' ';
    level[10] = '\0';
    snprintf(vol, sizeof vol, "%3u", (unsigned)m_pnl_vol[ch]);
    snprintf(pan, sizeof pan, "%3u", (unsigned)m_pnl_pan[ch]);

    p = snprintf(out, cap, "%2u %-16.16s %-4.4s %3.3s %-10.10s %3.3s %3.3s",
                 (unsigned)(ch + 1), inst, note, vel, level, vol, pan);
    return (uint8_t)p;
}

static void pnl_emit(uint8_t ch) {
    char out[64];
    uint8_t len = pnl_row(out, sizeof out, ch);

    pnl_goto((uint16_t)(PN_ROW_CH0 + ch));
    pnl_color(ch);
    console_write((const uint8_t *)out, len);
    console_puts("\x1b[K\x1b[0m");
}

static void pnl_sync(uint8_t ch) {
    m_pnl_sactive[ch] = m_pnl_active[ch];
    m_pnl_snote[ch] = m_pnl_note[ch];
    m_pnl_svel[ch] = m_pnl_vel[ch];
    m_pnl_sprog[ch] = pnl_eprog(ch);
    m_pnl_svol[ch] = m_pnl_vol[ch];
    m_pnl_span[ch] = m_pnl_pan[ch];
}

static bool pnl_changed(uint8_t ch) {
    return m_pnl_active[ch] != m_pnl_sactive[ch] ||
           m_pnl_note[ch] != m_pnl_snote[ch] ||
           m_pnl_vel[ch] != m_pnl_svel[ch] ||
           pnl_eprog(ch) != m_pnl_sprog[ch] ||
           m_pnl_vol[ch] != m_pnl_svol[ch] ||
           m_pnl_pan[ch] != m_pnl_span[ch];
}

// Repaint the title / song / info / column-header rows.  Called once at the
// start and whenever the elapsed second or tempo changes.
static void pnl_draw_header(void) {
    char line[80], et[8], tt[8];
    uint64_t elapsed = pnl_elapsed_us();
    uint16_t sec = (uint16_t)(elapsed / 1000000ULL);
    uint16_t bpm = m_ppqn ? (uint16_t)(60000000u / (m_us_num ? m_us_num : 1u))
                          : 0;

    pnl_time(elapsed, et);
    if (m_pnl_total_us) pnl_time(m_pnl_total_us, tt);
    else                strcpy(tt, "--:--");

    console_puts("\x1b[1;1H\x1b[7m z80neo MIDI player \x1b[0m\x1b[K");

    snprintf(line, sizeof line, " Song: %-58.58s",
             m_pnl_song[0] ? m_pnl_song : "(unknown)");
    pnl_goto(PN_ROW_SONG); console_puts(line); console_puts("\x1b[K");

    if (m_ppqn)
        snprintf(line, sizeof line,
                 " Time %s / %s    Tempo %u BPM    Tracks %u",
                 et, tt, (unsigned)bpm, (unsigned)m_ntrk);
    else
        snprintf(line, sizeof line,
                 " Time %s / %s    Tempo (SMPTE)    Tracks %u",
                 et, tt, (unsigned)m_ntrk);
    pnl_goto(PN_ROW_INFO); console_puts(line); console_puts("\x1b[K");

    snprintf(line, sizeof line,
             "%2.2s %-16.16s %-4.4s %3.3s %-10.10s %3.3s %3.3s",
             "CH", "INSTRUMENT", "NOTE", "VEL", "LEVEL", "VOL", "PAN");
    pnl_goto(PN_ROW_COLS); console_puts(line); console_puts("\x1b[K");

    m_pnl_hdr_sec = sec;
    m_pnl_hdr_bpm = bpm;
}

static void pnl_begin(void) {
    for (uint8_t ch = 0; ch < MIDI_PANEL_CH; ch++) {
        m_pnl_active[ch] = false;
        m_pnl_note[ch] = 0;
        m_pnl_vel[ch] = 0;
        m_pnl_prog[ch] = 0;
        m_pnl_hasprog[ch] = false;
        m_pnl_vol[ch] = 100;                     // MIDI defaults
        m_pnl_pan[ch] = 64;
        m_pnl_sactive[ch] = false;
        m_pnl_snote[ch] = 0;
        m_pnl_svel[ch] = 0;
        m_pnl_sprog[ch] = 0xFF;
        m_pnl_svol[ch] = 100;
        m_pnl_span[ch] = 64;
    }
    m_pnl_dirty = 0;
    m_pnl_pend = false;
    m_pnl_ev_n = 0;
    m_pnl_last_us = time_us_32();
    m_pnl_start_us = 0;
    m_pnl_time_started = false;
    m_pnl_hdr_sec = 0xFFFF;
    m_pnl_hdr_bpm = 0xFFFF;

    console_puts("\x1b[2J\x1b[H\x1b[0m");        // clear screen, home, reset attrs
    m_pnl_on = true;
    pnl_draw_header();
    for (uint8_t ch = 0; ch < MIDI_PANEL_CH; ch++) {
        pnl_emit(ch);
        pnl_sync(ch);
    }
}

static void pnl_end(void) {
    if (m_pnl_on)
        console_puts("\x1b[2J\x1b[H\x1b[0m");    // wipe the monitor, home the cursor
    m_pnl_on = false;
    m_pnl_want = false;
    m_pnl_pend = false;
    m_pnl_ev_n = 0;
}

static void pnl_apply(uint8_t st, uint8_t d1, uint8_t d2) {
    uint8_t ch = st & 0x0F;
    uint8_t type = st & 0xF0;

    if (ch >= MIDI_PANEL_CH) return;
    if (type == 0x90) {                          // note on (vel 0 = note off)
        if (d2 != 0) {
            m_pnl_note[ch] = d1;
            m_pnl_vel[ch] = d2;
            m_pnl_active[ch] = true;
            m_pnl_dirty |= (uint16_t)(1u << ch);
        } else if (m_pnl_active[ch] && m_pnl_note[ch] == d1) {
            m_pnl_active[ch] = false;
            m_pnl_dirty |= (uint16_t)(1u << ch);
        }
    } else if (type == 0x80) {                   // note off
        if (m_pnl_active[ch] && m_pnl_note[ch] == d1) {
            m_pnl_active[ch] = false;
            m_pnl_dirty |= (uint16_t)(1u << ch);
        }
    } else if (type == 0xC0) {                   // program change (1 data byte)
        if (!m_pnl_hasprog[ch] || m_pnl_prog[ch] != d1) {
            m_pnl_prog[ch] = d1;
            m_pnl_hasprog[ch] = true;
            m_pnl_dirty |= (uint16_t)(1u << ch);
        }
    } else if (type == 0xB0) {
        if (d1 == 7) {                           // channel volume
            if (m_pnl_vol[ch] != d2) {
                m_pnl_vol[ch] = d2;
                m_pnl_dirty |= (uint16_t)(1u << ch);
            }
        } else if (d1 == 10) {                   // pan
            if (m_pnl_pan[ch] != d2) {
                m_pnl_pan[ch] = d2;
                m_pnl_dirty |= (uint16_t)(1u << ch);
            }
        } else if (d1 == 120 || d1 == 123) {     // all sound / notes off
            if (m_pnl_active[ch]) {
                m_pnl_active[ch] = false;
                m_pnl_dirty |= (uint16_t)(1u << ch);
            }
        }
    }
}

static void pnl_flush_pending(void) {
    if (!m_pnl_pend) return;
    for (uint8_t i = 0; i < m_pnl_ev_n; i++)
        pnl_apply(m_pnl_ev_st[i], m_pnl_ev_d1[i], m_pnl_ev_d2[i]);
    m_pnl_ev_n = 0;
    m_pnl_pend = false;
}

static void pnl_push(uint8_t st, uint8_t d1, uint8_t d2) {
    if (m_pnl_ev_n < MIDI_PANEL_EV_MAX) {
        m_pnl_ev_st[m_pnl_ev_n] = st;
        m_pnl_ev_d1[m_pnl_ev_n] = d1;
        m_pnl_ev_d2[m_pnl_ev_n] = d2;
        m_pnl_ev_n++;
    }
}

static void pnl_schedule(uint64_t due_us) {
    if (m_pnl_ev_n == 0) return;
    m_pnl_due_us = due_us;
    m_pnl_pend = true;
}

void midi_panel_poll(void) {
    if (!m_pnl_on) return;

    if (m_pnl_pend && time_us_64() >= m_pnl_due_us)
        pnl_flush_pending();

    // Refresh the header when the elapsed second or tempo changes (~1 Hz).
    {
        uint64_t elapsed = pnl_elapsed_us();
        uint16_t sec = (uint16_t)(elapsed / 1000000ULL);
        uint16_t bpm = m_ppqn
                     ? (uint16_t)(60000000u / (m_us_num ? m_us_num : 1u)) : 0;
        if (sec != m_pnl_hdr_sec || bpm != m_pnl_hdr_bpm)
            pnl_draw_header();
    }

    if (m_pnl_dirty == 0) return;
    uint32_t now = time_us_32();
    if ((uint32_t)(now - m_pnl_last_us) < MIDI_PANEL_INTERVAL_US) return;
    m_pnl_last_us = now;

    uint8_t budget = MIDI_PANEL_ROWS_PER_TICK;
    for (uint8_t ch = 0; ch < MIDI_PANEL_CH && budget; ch++) {
        if ((m_pnl_dirty & (uint16_t)(1u << ch)) == 0) continue;
        m_pnl_dirty &= (uint16_t)~(1u << ch);
        if (!pnl_changed(ch)) continue;          // already showing this
        pnl_emit(ch);
        pnl_sync(ch);
        budget--;
    }
}

void midi_panel_set_enabled(bool on) { m_pnl_enabled = on; }
bool midi_panel_enabled(void) { return m_pnl_enabled; }

// Build one grouped record: delay, a raw MIDI byte count, then the raw bytes of
// every channel message sharing the earliest pending tick.  Tempo and end-of-
// track events are consumed for their timing only (count 0 records).  A tick
// needing more than MIDI_GROUP_MAX bytes is split; the extra records get delay
// 0 because m_global_tick is already at that tick.
//
// The record's absolute due time is tracked in microseconds on the RP2350
// timer.  The delay handed to the Z80 is `due - now`, recomputed on every
// record, so the Z80's replay cost and any wait-loop error are absorbed rather
// than accumulating.  A gap longer than the 16-bit millisecond delay field is
// split into delay-only records without consuming the event, so its bytes are
// only released when the tick is actually due.
static void m_produce_group(void) {
    uint32_t bt;
    uint16_t used = 0;
    uint64_t due, now, rem, new_rem = 0;

    int8_t best = m_best_track(&bt);
    if (best < 0) {
        m_grp[0] = 0;
        m_grp[1] = 0;
        m_grp[2] = 0xFF;                       // end sentinel
        m_grp_len = 3;
        m_grp_pos = 0;
        return;
    }

    // Draw the panel lazily on the first real record: by now the player has
    // printed its "Playing" line, so the panel lands below it rather than in
    // the middle of the open sequence.
    if (m_pnl_want) {
        if (!m_pnl_on) pnl_begin();
        pnl_flush_pending();
    }

    if (!m_started) {
        m_started = true;
        m_base_us = time_us_64();              // anchor tick 0 to playback start
    }
    due = m_base_us + m_delta_us(m_global_tick, bt, &new_rem);

    // The header clock starts when the first record is actually due, so a long
    // initial gap is not counted as elapsed time.
    if (m_pnl_want && !m_pnl_time_started) {
        m_pnl_start_us = due;
        m_pnl_time_started = true;
    }

    now = time_us_64();
    if (due > now && due - now > 65535000ULL) {
        m_grp[0] = 0xFF;                       // delay-only: 65535 ms
        m_grp[1] = 0xFF;
        m_grp[2] = 0;
        m_grp_len = 3;
        m_grp_pos = 0;
        return;                                // event stays pending
    }

    m_global_tick = bt;                        // the tick now being collected

    // Collect every event at this tick.  A channel message that will not fit is
    // left pending; since m_global_tick is already bt, the next record's delay
    // is 0 and the bytes still land on time.
    for (;;) {
        mtrack_t *t;
        uint8_t mlen;

        best = m_best_track(&bt);
        if (best < 0 || bt != m_global_tick)
            break;
        t = &m_trk[best];

        if (t->pend == 1) {
            mlen = t->dlen;
            if ((uint16_t)(used + mlen) > MIDI_GROUP_MAX)
                break;                         // split at the next record
            m_grp[3 + used++] = t->st;
            m_grp[3 + used++] = t->d1;
            if (mlen == 3)
                m_grp[3 + used++] = t->d2;
            if (m_pnl_want)
                pnl_push(t->st, t->d1, t->d2);
        } else if (t->pend == 2) {
            m_tempo_us = ((uint32_t)t->tempo[0] << 16) |
                         ((uint32_t)t->tempo[1] << 8) |
                         (uint32_t)t->tempo[2];
            if (m_ppqn)
                m_us_num = m_tempo_us;         // denominator is the tick division
        }
        m_read_event(t);
    }

    // Commit the tick and hold the record until it is due.  The optional trim
    // (tenths of a millisecond) biases the whole stream faster; 0 by default.
    m_tick_rem = new_rem;
    now = time_us_64();
    rem = due > now ? due - now : 0;
    {
        uint64_t trim_us = ((uint64_t)midi_credit_group_x10 +
                            (uint64_t)used * midi_credit_byte_x10) * 100ULL;
        rem = rem > trim_us ? rem - trim_us : 0;
    }

    uint32_t ms = (uint32_t)(rem / 1000ULL);
    if (ms > 65535u)
        ms = 65535u;

    m_grp[0] = (uint8_t)ms;
    m_grp[1] = (uint8_t)(ms >> 8);
    m_grp[2] = (uint8_t)used;                  // 0 = delay-only record
    m_grp_len = (uint16_t)(3 + used);
    m_grp_pos = 0;

    if (m_pnl_want)
        pnl_schedule(due);
    m_base_us = due;
}

// Pre-scan the merged timeline to find the song's total duration for the
// header clock.  It runs only when the whole file is in PSRAM (a full scan
// through the streaming SD path would stall the open for seconds) and returns 0
// when the duration is unknown.  The parser state is saved and restored so
// playback is unaffected.
static uint64_t m_scan_total_us(void) {
    static mtrack_t save[MIDI_MAX_TRACKS];
    uint8_t  sv_ntrk = m_ntrk;
    uint32_t sv_gtick = m_global_tick;
    uint64_t sv_rem = m_tick_rem;
    uint32_t sv_num = m_us_num, sv_den = m_us_den, sv_tempo = m_tempo_us;
    bool     sv_ppqn = m_ppqn;
    uint64_t total = 0;
    uint32_t cur = 0;

    memcpy(save, m_trk, sizeof(m_trk));

    for (;;) {
        uint32_t bt;
        int8_t best = m_best_track(&bt);
        if (best < 0) break;
        total += (uint64_t)(bt - cur) * (uint64_t)m_us_num / m_us_den;
        cur = bt;
        if (m_trk[best].pend == 2) {
            m_tempo_us = ((uint32_t)m_trk[best].tempo[0] << 16) |
                         ((uint32_t)m_trk[best].tempo[1] << 8) |
                         (uint32_t)m_trk[best].tempo[2];
            if (m_ppqn) m_us_num = m_tempo_us;
        }
        m_read_event(&m_trk[best]);
    }

    memcpy(m_trk, save, sizeof(m_trk));
    m_ntrk = sv_ntrk;
    m_global_tick = sv_gtick;
    m_tick_rem = sv_rem;
    m_us_num = sv_num;
    m_us_den = sv_den;
    m_tempo_us = sv_tempo;
    m_ppqn = sv_ppqn;
    return total;
}

bool midi_is_open(void) {
    return m_open;
}

bool midi_open(const char *name) {
    uint8_t h[14];
    uint32_t hlen, p;
    uint16_t fmt, nt, div;
    UINT br = 0;

    midi_close();

    if (!name || !name[0]) return false;
    if (!fileio_ensure_mount()) return false;
    if (f_open(&m_fil, name, FA_READ) != FR_OK) return false;

    // Validate the SMF header before loading the whole-file image.  Play-all
    // walks every entry in a directory, and reading a large non-MIDI file (an
    // 845 KB CPMDISK image, say) into PSRAM only to reject it makes the pass
    // crawl; a 14-byte header read is enough to skip it.
    if (f_lseek(&m_fil, 0) != FR_OK ||
        f_read(&m_fil, h, sizeof h, &br) != FR_OK || br < sizeof h) {
        f_close(&m_fil);
        return false;
    }
    if (memcmp(h, "MThd", 4) != 0) {
        f_close(&m_fil);
        return false;
    }
    hlen = be32(h + 4);
    fmt = be16(h + 8);
    nt = be16(h + 10);
    div = be16(h + 12);
    (void)nt;
    // Type 2 is independent sequences, which the by-tick merge would scramble.
    if (hlen < 6 || fmt > 1) {
        f_close(&m_fil);
        return false;
    }

    // Prefer a whole-file image: an f_lseek + 512-byte refill on this card can
    // cost seconds, and even one per track would dominate playback.  The image
    // lives in PSRAM (see MIDI_IMG_MAX); fall back to the streaming buffer when
    // there is no PSRAM or the file is too big for the reserved region.
    m_img = NULL;
    m_size = (uint32_t)f_size(&m_fil);
    if (m_size && psram_base && m_size <= MIDI_IMG_MAX) {
        uint8_t *img = psram_base + (PSRAM_BANKS * RAM_SIZE);
        UINT got = 0;
        if (f_lseek(&m_fil, 0) == FR_OK &&
            f_read(&m_fil, img, m_size, &got) == FR_OK && got == m_size)
            m_img = img;
    }

    m_ntrk = 0;
    p = 8 + hlen;
    while (m_ntrk < MIDI_MAX_TRACKS) {
        uint8_t ch[8];
        uint32_t len;
        mtrack_t *t;

        if (f_lseek(&m_fil, p) != FR_OK ||
            f_read(&m_fil, ch, sizeof ch, &br) != FR_OK || br < sizeof ch)
            break;
        if (memcmp(ch, "MTrk", 4) != 0)
            break;
        len = be32(ch + 4);
        if (len == 0) { p += 8; continue; }

        t = &m_trk[m_ntrk++];
        memset(t, 0, sizeof *t);
        t->pos = p + 8;
        t->end = p + 8 + len;
        p += 8 + len;
    }
    if (m_ntrk == 0) {
        m_img = NULL;
        f_close(&m_fil);
        return false;
    }

    m_ppqn = 0;
    m_tempo_us = 500000u;
    m_global_tick = 0;
    m_base_us = 0;
    m_tick_rem = 0;
    m_started = false;
    m_grp_pos = m_grp_len = 0;                 // force a produce on first read
    if (div & 0x8000) {
        int16_t f = (int16_t)(div >> 8);
        uint8_t tpf = (uint8_t)(div & 0xFF);
        if (f < 0) f = (int16_t)(-f);
        if (f && tpf) {
            m_us_num = 1000000u;
            m_us_den = (uint32_t)f * (uint32_t)tpf;
        } else {
            m_us_num = 5000u;
            m_us_den = 1u;
        }
    } else if (div) {
        m_ppqn = 1;
        m_us_num = m_tempo_us;
        m_us_den = div;
    } else {
        m_us_num = 5208u;                      // malformed division: 120 bpm-ish
        m_us_den = 1u;
    }

    for (uint8_t i = 0; i < m_ntrk; i++)
        m_read_event(&m_trk[i]);

    // Copy the song name and, when the whole file is in PSRAM, work out the
    // total duration for the header clock (0 = unknown).
    snprintf(m_pnl_song, sizeof m_pnl_song, "%s", name);
    m_pnl_total_us = m_img ? m_scan_total_us() : 0;

    m_pnl_on = false;              // drawn lazily on the first produced record
    m_pnl_want = m_pnl_enabled;
    m_pnl_pend = false;
    m_pnl_ev_n = 0;

    m_open = true;
    return true;
}

void midi_close(void) {
    pnl_end();
    m_img = NULL;                  // PSRAM, not heap: nothing to free
    m_size = 0;
    if (m_open) {
        f_close(&m_fil);
        m_open = false;
    }
    m_ntrk = 0;
    m_tick_rem = 0;
    m_started = false;
    m_grp_pos = m_grp_len = 0;
}

uint8_t midi_read_byte(void) {
    if (!m_open) return 0xFF;

    if (m_grp_pos >= m_grp_len)
        m_produce_group();
    return m_grp[m_grp_pos++];
}
