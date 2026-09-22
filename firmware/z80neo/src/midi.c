// midi.c — firmware-side Standard MIDI File parser/streamer
// z80neo — TurBoss 2026
//
// Parses an SMF from the SD card (FatFS) and merges its tracks into a stream
// of compact grouped records for the Z80 MIDI player (see include/midi.h).
// The Pico does the parsing so the 128 kHz Z80 only has to wait and emit; the
// Z80-side C parser could not keep up.  Kept read-only, one file at a time.

#include "midi.h"

#include <stdlib.h>
#include <string.h>

#include <pico/stdlib.h>

#include "ff.h"
#include "fileio.h"

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

static FIL      m_fil;
static uint8_t *m_img;          // whole file, or NULL to stream from SD
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

    if (!m_started) {
        m_started = true;
        m_base_us = time_us_64();              // anchor tick 0 to playback start
    }
    due = m_base_us + m_delta_us(m_global_tick, bt, &new_rem);

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

    m_base_us = due;
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

    // Prefer a whole-file image: an f_lseek + 512-byte refill on this card can
    // cost seconds, and even one per track would dominate playback.  Fall back
    // to the streaming buffer if the file does not fit in the heap.
    m_img = NULL;
    m_size = (uint32_t)f_size(&m_fil);
    if (m_size) {
        m_img = (uint8_t *)malloc(m_size);
        if (m_img) {
            UINT got = 0;
            if (f_lseek(&m_fil, 0) != FR_OK ||
                f_read(&m_fil, m_img, m_size, &got) != FR_OK || got != m_size) {
                free(m_img);
                m_img = NULL;
            }
        }
    }

    if (f_lseek(&m_fil, 0) != FR_OK ||
        f_read(&m_fil, h, sizeof h, &br) != FR_OK || br < sizeof h) {
        if (m_img) { free(m_img); m_img = NULL; }
        f_close(&m_fil);
        return false;
    }
    if (memcmp(h, "MThd", 4) != 0) {
        if (m_img) { free(m_img); m_img = NULL; }
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
        if (m_img) { free(m_img); m_img = NULL; }
        f_close(&m_fil);
        return false;
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
        if (m_img) { free(m_img); m_img = NULL; }
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

    m_open = true;
    return true;
}

void midi_close(void) {
    if (m_img) {
        free(m_img);
        m_img = NULL;
        m_size = 0;
    }
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
