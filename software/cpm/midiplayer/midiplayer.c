/*
 * midiplayer.c - z80neo CP/M Standard MIDI File player
 *
 * A CP/M 2.2 transient (.COM) built with z88dk's +cpm target.  Plays a .mid out
 * the second UART the firmware exposes on ports 0x90 (data) / 0x91 (status) at
 * the MIDI rate, 31250 baud 8N1.  It is the CP/M twin of the bare-metal
 * software/z80neo/midiplayer; the Pico still does all SMF parsing.
 *
 * The file API's FILE_OPEN_MIDI command (0xA0) turns port 0xA2 into a stream of
 * grouped records (see firmware/z80neo/include/midi.h):
 *
 *     [0] delay lo   } milliseconds before this record
 *     [1] delay hi   }
 *     [2] byte count 0 = delay only, 0xFF = end, else raw bytes follow
 *     [3..]          byte count raw MIDI message bytes, forwarded as-is
 *
 * The Z80 only waits and emits; the replay loop is the shared
 * software/z80neo/midiplayer/midi_delay.asm (no UI, so nothing runs between
 * records and the absolute scheduler keeps the stream even).
 *
 * SD volume care: the firmware's file API and the CP/M disk layer share the
 * same FatFS volume, so any file/dir access here displaces the emulated disk
 * mount. After the file is closed we issue FILE_REMOUNT_CPM (command 9) to hand
 * the volume back and re-open the CPMDISK images, or the CCP would fail disk
 * I/O on return.  This needs firmware that implements command 9.
 *
 * Usage:  MIDI [file.mid]
 *   With no argument it lists /MIDI (or the card root) and prompts.
 *   A name may be a number, a bare name, or a path such as /MIDI/song.mid.
 *
 * Build:  make            -> build/MIDI.COM
 * Deploy: copy MIDI.COM into a CPMDISK image (A:), not the FAT root.
 */

#include <cpm.h>
#include <stdint.h>

/* ---- Ports ------------------------------------------------------------- */

__sfr __at 0x90 MIDI_DATA;
__sfr __at 0x91 MIDI_STATUS; /* bit0 = RX ready, bit1 = TX ready */
__sfr __at 0x92 MIDI_CFG_BAUD;
__sfr __at 0x93 MIDI_CFG_FMT;

__sfr __at 0xA0 FILE_CMD;
__sfr __at 0xA1 FILE_NAME;
__sfr __at 0xA2 FILE_DATA;
__sfr __at 0xA3 FILE_RST;

#define FCMD_CLOSE 2
#define FCMD_DIRRESET 5
#define FCMD_DIRNEXT 6
#define FCMD_OPEN_MIDI 8
#define FCMD_REMOUNT_CPM 9

#define FST_EOF 0x02
#define FST_ERR 0x04
#define FST_OPEN 0x08
#define FST_NAME 0x10
#define FST_DIR 0x40

#define MIDI_BAUD_CODE 10 /* 31250 in the firmware table */
#define MIDI_FMT_8N1 0x03 /* data bits 8, stop 1, parity none */

/* Preferred directory on the SD card; falls back to the root if absent. */
#define MIDI_DIR "/MIDI"

#define CMD_TAIL 0x0080

/* Calibrated busy-wait and record replay loop (shared with the bare-metal
 * player, software/z80neo/midiplayer/midi_delay.asm). */
extern void midi_delay_ms(uint16_t ms);
extern void midi_play(void);
/* Set by midi_play: 0 = end of song, 1 = 'q'/ESC. */
extern uint8_t midi_stopped;

/* ---- Console (BDOS direct console I/O) --------------------------------- */

static void con_put(uint8_t c) { bdos(CPM_DCIO, c); }

static void con_puts(const char *s) {
	while (*s)
		con_put((uint8_t)*s++);
}

/* Non-blocking key read; 0 when nothing is waiting. */
static int con_get(void) { return bdos(CPM_DCIO, 0xFF); }

static void con_num(uint16_t v) {
	char b[6];
	uint8_t n = 0;

	if (v == 0) {
		con_put('0');
		return;
	}
	while (v) {
		b[n++] = (char)('0' + (v % 10));
		v /= 10;
	}
	while (n)
		con_put((uint8_t)b[--n]);
}

static void con_hex8(uint8_t v) {
	static const char hex[] = "0123456789ABCDEF";
	con_put((uint8_t)hex[v >> 4]);
	con_put((uint8_t)hex[v & 0x0F]);
}

/* Counted while read_line waits; seeds the random play order (human keypress
 * timing is the only entropy this machine has). */
static uint16_t g_wait_ticks;

/* Blocking line read with local echo and backspace. */
static void read_line(char *buf, uint8_t max) {
	uint8_t n = 0;
	int c;

	for (;;) {
		while ((c = con_get()) == 0)
			g_wait_ticks++;
		if (c == '\r' || c == '\n')
			break;
		if (c == 8 || c == 127) {
			if (n) {
				n--;
				con_puts("\b \b");
			}
			continue;
		}
		if (c >= 32 && n < (uint8_t)(max - 1)) {
			buf[n++] = (char)c;
			con_put((uint8_t)c);
		}
	}
	buf[n] = '\0';
	con_puts("\r\n");
}

/* ---- MIDI out ---------------------------------------------------------- */

static uint8_t g_old_baud;
static uint8_t g_old_fmt;
static uint8_t g_have_cfg;

static void midi_init(void) {
	/* Save the current UART1 line so it can be put back on exit.  A read of
	 * 0x93 sets bit7 when the firmware implements the config ports. */
	g_old_baud = MIDI_CFG_BAUD;
	g_old_fmt = MIDI_CFG_FMT;
	g_have_cfg = (uint8_t)((g_old_fmt & 0x80) != 0);
	/* Format first (with the current baud), then the MIDI baud code, so the
	 * final configure uses 31250 8N1. */
	MIDI_CFG_FMT = MIDI_FMT_8N1;
	MIDI_CFG_BAUD = MIDI_BAUD_CODE;
}

static void midi_restore(void) {
	if (!g_have_cfg)
		return;
	MIDI_CFG_FMT = g_old_fmt; /* bit7 is ignored by the format decode */
	MIDI_CFG_BAUD = g_old_baud;
}

/* ---- Directory scan / file selection ----------------------------------- */

#define NAME_MAX 128
#define PAGE_LINES 20

static char g_dir[NAME_MAX]; /* current directory, "" = root */

/* Reset the scan to `dir` and skip the first `skip` entries.  Returns 0 when
 * the directory does not open or holds fewer than `skip` entries.  The list is
 * never held in memory, so any number of entries can be listed. */
static uint8_t dir_start(const char *dir, uint16_t skip) {
	const char *p;

	FILE_RST = 0;
	for (p = dir; *p; p++)
		FILE_NAME = (uint8_t)*p;
	FILE_CMD = FCMD_DIRRESET;
	if (FILE_CMD & FST_ERR)
		return 0;
	while (skip--) {
		FILE_CMD = FCMD_DIRNEXT;
		if ((FILE_CMD & FST_NAME) == 0)
			return 0;
	}
	return 1;
}

/* Advance one entry and copy its name to `out`, setting *is_dir.  0 at end. */
static uint8_t dir_next(char *out, uint8_t max, uint8_t *is_dir) {
	uint8_t i = 0, c, s;

	FILE_CMD = FCMD_DIRNEXT;
	s = FILE_CMD;
	if (is_dir)
		*is_dir = (uint8_t)((s & FST_DIR) != 0);
	if ((s & FST_NAME) == 0)
		return 0;
	while ((c = FILE_NAME) != 0 && i < (uint8_t)(max - 1))
		out[i++] = (char)c;
	out[i] = '\0';
	return i;
}

/* Join `name` onto `dir`; absolute when `name` starts with '/'.  "" = root. */
static void path_join(char *out, uint8_t max, const char *dir, const char *name) {
	uint8_t n = 0;
	const char *p;

	if (name[0] == '/') {
		for (p = name; *p && n < (uint8_t)(max - 1); p++)
			out[n++] = *p;
		out[n] = '\0';
		return;
	}
	if (dir[0]) {
		for (p = dir; *p && n < (uint8_t)(max - 1); p++)
			out[n++] = *p;
		if (n && out[n - 1] != '/' && n < (uint8_t)(max - 1))
			out[n++] = '/';
	}
	for (p = name; *p && n < (uint8_t)(max - 1); p++)
		out[n++] = *p;
	out[n] = '\0';
}

/* Build the full path for `name` relative to the current directory. */
static void make_path(char *out, uint8_t max, const char *name) {
	path_join(out, max, g_dir, name);
}

/* Drop the last path component ("/a/b" -> "/a", "/x" -> ""). */
static void dir_up(char *dir) {
	uint8_t n = 0, last = 0;

	while (dir[n]) {
		if (dir[n] == '/')
			last = n;
		n++;
	}
	if (n == 0)
		return;
	dir[last] = '\0';
}

/* Print entries [page*PAGE_LINES, ...); directories get a trailing '/'.
 * Returns how many were shown. */
static uint8_t show_page(const char *dir, uint16_t page) {
	char nm[NAME_MAX];
	uint16_t idx = (uint16_t)(page * PAGE_LINES);
	uint8_t shown = 0, is_dir = 0;

	if (!dir_start(dir, (uint16_t)(page * PAGE_LINES)))
		return 0;
	while (shown < PAGE_LINES && dir_next(nm, sizeof nm, &is_dir)) {
		con_num((uint16_t)(idx + 1));
		con_puts(") ");
		con_puts(nm);
		if (is_dir)
			con_put('/');
		con_puts("\r\n");
		idx++;
		shown++;
	}
	return shown;
}

/* Fetch the 1-based `n`-th entry.  Returns 1 on success. */
static uint8_t nth_entry(const char *dir, uint16_t n, char *out, uint8_t max,
						 uint8_t *is_dir) {
	if (n == 0 || !dir_start(dir, (uint16_t)(n - 1)))
		return 0;
	return dir_next(out, max, is_dir) != 0;
}

static uint8_t dir_has_entries(const char *dir) {
	char nm[NAME_MAX];
	uint8_t d;
	return (uint8_t)(dir_start(dir, 0) && dir_next(nm, sizeof nm, &d) != 0);
}

/* Number of entries in `dir` (files + subdirectories). */
static uint16_t dir_count(const char *dir) {
	char nm[NAME_MAX];
	uint16_t n = 0;
	uint8_t d;

	if (!dir_start(dir, 0))
		return 0;
	while (dir_next(nm, sizeof nm, &d))
		n++;
	return n;
}

/* ---- Random order ------------------------------------------------------ */

/* xorshift16, seeded from the human keypress timing in read_line(). */
static uint16_t g_rng = 0xACE1u;

static uint16_t rng_next(void) {
	g_rng ^= (uint16_t)(g_rng << 7);
	g_rng ^= (uint16_t)(g_rng >> 9);
	g_rng ^= (uint16_t)(g_rng << 8);
	return g_rng;
}

static uint16_t gcd16(uint16_t a, uint16_t b) {
	while (b) {
		uint16_t t = (uint16_t)(a % b);
		a = b;
		b = t;
	}
	return a;
}

/* A stride coprime to `count`, so `(start + i*stride) % count` visits every
 * entry exactly once (a memory-free shuffle). */
static uint16_t pick_stride(uint16_t count) {
	uint16_t s;

	if (count < 3)
		return 1;
	for (;;) {
		s = (uint16_t)(rng_next() % (uint16_t)(count - 1)) + 1;
		if (gcd16(s, count) == 1)
			return s;
	}
}

/* Browse actions returned to main(). */
#define ACT_QUIT 0
#define ACT_FILE 1
#define ACT_ALL 2
#define ACT_ALL_RPT 3
#define ACT_RAND 4
#define ACT_RAND_RPT 5

/* Tree browser.  A number opens a file or descends into a directory, ".." goes
 * up, n/p page, a/A play all (once/loop), r/R play randomly (once/loop), q
 * quits, and anything else is taken as a filename.  On a file, returns ACT_FILE
 * with its name in `out`; g_dir is updated in place when navigating. */
static uint8_t browse(char *out) {
	char buf[NAME_MAX];
	char nm[NAME_MAX];
	char path[NAME_MAX];
	uint16_t page = 0;
	uint8_t i, digits, shown, is_dir;
	uint16_t n;

	for (;;) {
		shown = show_page(g_dir, page);
		if (shown == 0 && page > 0) { /* ran past the end: step back */
			page--;
			continue;
		}
		con_puts("Dir ");
		con_puts(g_dir[0] ? g_dir : "/");
		con_puts("  page ");
		con_num((uint16_t)(page + 1));
		con_puts("  (num, .., n/p, a/A all, r/R rand, q): ");
		read_line(buf, sizeof buf);

		if ((buf[0] == 'q' || buf[0] == 'Q') && buf[1] == '\0')
			return ACT_QUIT;
		if (buf[0] == '.' && buf[1] == '.' && buf[2] == '\0') {
			dir_up(g_dir);
			page = 0;
			continue;
		}
		if ((buf[0] == 'n' || buf[0] == 'N') && buf[1] == '\0') {
			page++;
			continue;
		}
		if ((buf[0] == 'p' || buf[0] == 'P') && buf[1] == '\0') {
			if (page)
				page--;
			continue;
		}
		if (buf[1] == '\0' && (buf[0] == 'a' || buf[0] == 'A'))
			return (buf[0] == 'a') ? ACT_ALL : ACT_ALL_RPT;
		if (buf[1] == '\0' && (buf[0] == 'r' || buf[0] == 'R'))
			return (buf[0] == 'r') ? ACT_RAND : ACT_RAND_RPT;
		if (buf[0] == '\0')
			continue; /* blank: redraw */

		digits = 1;
		for (i = 0; buf[i]; i++)
			if (buf[i] < '0' || buf[i] > '9') {
				digits = 0;
				break;
			}
		if (digits) {
			n = 0;
			for (i = 0; buf[i]; i++)
				n = (uint16_t)(n * 10 + (uint16_t)(buf[i] - '0'));
			if (!nth_entry(g_dir, n, nm, sizeof nm, &is_dir)) {
				con_puts("No such entry.\r\n");
				continue;
			}
			if (is_dir) { /* descend, staying inside g_dir */
				path_join(path, sizeof path, g_dir, nm);
				for (i = 0; path[i] && i < (uint8_t)(sizeof g_dir - 1); i++)
					g_dir[i] = path[i];
				g_dir[i] = '\0';
				page = 0;
				continue;
			}
			for (i = 0; nm[i]; i++)
				out[i] = nm[i];
			out[i] = '\0';
			return ACT_FILE;
		}
		/* anything else is a filename (relative to g_dir, or absolute) */
		for (i = 0; buf[i]; i++)
			out[i] = buf[i];
		out[i] = '\0';
		return ACT_FILE;
	}
}

/* Start in /MIDI when it exists, else the card root.  Returns 1 when the
 * chosen directory has entries, 0 otherwise. */
static uint8_t pick_start_dir(void) {
	g_dir[0] = '\0';
	if (dir_has_entries(MIDI_DIR)) {
		uint8_t i = 0;
		const char *p = MIDI_DIR;
		while (p[i] && i < (uint8_t)(sizeof g_dir - 1)) {
			g_dir[i] = p[i];
			i++;
		}
		g_dir[i] = '\0';
		return 1;
	}
	return dir_has_entries("");
}

/* ---- Playback ---------------------------------------------------------- */

#define PLAY_END 1  /* song reached its end sentinel */
#define PLAY_STOP 0 /* user pressed q/ESC */
#define PLAY_FAIL 2 /* could not open */

/* Open and play one file.  Returns PLAY_END / PLAY_STOP / PLAY_FAIL. */
static uint8_t play_file(const char *path) {
	const char *p;
	uint8_t s;

	con_puts("Opening ");
	con_puts(path);
	con_puts(" ... ");
	FILE_RST = 0;
	for (p = path; *p; p++)
		FILE_NAME = (uint8_t)*p;
	FILE_CMD = FCMD_OPEN_MIDI;
	s = FILE_CMD;
	if ((s & FST_OPEN) == 0) {
		con_puts("failed (status 0x");
		con_hex8(s);
		con_puts(")\r\n\r\n");
		FILE_CMD = FCMD_CLOSE;
		return PLAY_FAIL;
	}
	con_puts("ok\r\nPlaying (q or ESC to stop)...\r\n");
	midi_play();
	FILE_CMD = FCMD_CLOSE;
	if (midi_stopped) {
		con_puts("\r\nStopped.\r\n\r\n");
		return PLAY_STOP;
	}
	return PLAY_END;
}

/* Files started by the last play_dir() call, for the "no files" message. */
static uint16_t g_played;

/* Depth-first play of every file at or under `dir`, descending into
 * subdirectories.  `dir` is a caller-owned buffer that stays valid across the
 * recursion (each frame's `path` holds the child path handed to the next
 * level), so g_dir is left untouched and the browser stays where it was.
 * Each directory keeps the existing coprime-stride shuffle when random_order.
 * Returns 1 when the user stopped playback. */
static uint8_t play_tree(const char *dir, uint8_t random_order) {
	char nm[NAME_MAX];
	char path[NAME_MAX + 16];
	uint16_t count = dir_count(dir);
	uint16_t i, idx;
	uint32_t start = 0, step = 1;
	uint8_t d;

	if (count == 0)
		return 0;
	if (random_order) {
		g_rng = (uint16_t)(g_rng ^ g_wait_ticks ^ count ^ 0x9E37u);
		if (g_rng == 0)
			g_rng = 1;
		step = pick_stride(count);
		start = (uint32_t)(rng_next() % count);
	}

	for (i = 0; i < count; i++) {
		idx = random_order
				  ? (uint16_t)((start + (uint32_t)i * step) % count)
				  : i;
		if (!nth_entry(dir, (uint16_t)(idx + 1), nm, sizeof nm, &d))
			continue;
		path_join(path, sizeof path, dir, nm);
		if (d) {
			if (play_tree(path, random_order))
				return 1;
		} else {
			g_played++;
			if (play_file(path) == PLAY_STOP)
				return 1;
		}
	}
	return 0;
}

/* Play every file in the current directory and all of its subdirectories, in
 * order or shuffled, optionally looping until the user stops. */
static void play_dir(uint8_t random_order, uint8_t repeat) {
	if (dir_count(g_dir) == 0) {
		con_puts("Empty directory.\r\n\r\n");
		return;
	}

	g_played = 0;
	do {
		if (play_tree(g_dir, random_order))
			break;
		if (g_played == 0) /* nothing playable under here */
			break;
	} while (repeat);

	if (g_played == 0)
		con_puts("No files found.\r\n\r\n");
}

/* ---- Command tail ------------------------------------------------------ */

static char g_arg[128];
static uint8_t g_have_arg;

static void parse_tail(void) {
	unsigned char *tail = (unsigned char *)CMD_TAIL;
	uint8_t n = tail[0];
	uint8_t i, j = 0;

	if (n > sizeof g_arg - 1)
		n = sizeof g_arg - 1;
	for (i = 0; i < n; i++) {
		char c = (char)tail[1 + i];
		if (j == 0 && (c == ' ' || c == '\t' || c == '\0'))
			continue;
		if (c == ' ' || c == '\t' || c == '\0' || c == '\r' || c == '\n')
			break;
		g_arg[j++] = c;
	}
	g_arg[j] = '\0';
	g_have_arg = (uint8_t)(j != 0);
}

/* ---- main -------------------------------------------------------------- */

int main(void) {
	char base[NAME_MAX];
	char path[NAME_MAX + 16];
	uint8_t act;

	parse_tail();

	con_puts("\r\nz80neo CP/M MIDI player\r\n");
	midi_init();
	con_puts("UART1 -> 31250 8N1 (MIDI); cfg code ");
	con_num((uint16_t)MIDI_CFG_BAUD);
	con_puts(MIDI_CFG_BAUD == MIDI_BAUD_CODE ? " ok"
											 : "  (need firmware code 10)");
	con_puts("\r\n\r\n");

	if (g_have_arg) {
		play_file(g_arg);
		FILE_CMD = FCMD_REMOUNT_CPM; /* hand the volume back to CP/M */
		midi_restore();
		return 0;
	}

	/* Pick a start directory, then browse and play in a loop, so the current
	 * directory persists across songs (no reset needed to walk the tree). */
	if (!pick_start_dir()) {
		con_puts("No files found in ");
		con_puts(MIDI_DIR);
		con_puts(" or the root\r\n");
		FILE_CMD = FCMD_REMOUNT_CPM;
		midi_restore();
		return 1;
	}

	for (;;) {
		act = browse(base);
		if (act == ACT_QUIT)
			break;
		if (act == ACT_FILE) {
			make_path(path, sizeof path, base);
			play_file(path);
		} else {
			play_dir((uint8_t)(act == ACT_RAND || act == ACT_RAND_RPT),
					 (uint8_t)(act == ACT_ALL_RPT || act == ACT_RAND_RPT));
		}
	}

	/* The player borrowed the SD volume; hand it back before returning to
	 * CP/M so the CCP's disk I/O works again. */
	FILE_CMD = FCMD_REMOUNT_CPM;
	midi_restore();
	con_puts("Bye.\r\n");
	return 0;
}
