/*
 * midiplayer.c - z80neo SD-card Standard MIDI File player
 *
 * Bare-metal +z80neo program (linked at 0x0000).  Plays a .mid out the second
 * UART the firmware exposes on ports 0x90 (data) / 0x91 (status) at the MIDI
 * rate, 31250 baud 8N1.
 *
 * The Pico parses the SMF and merges its tracks; the Z80 only has to wait and
 * emit.  The file API's FILE_OPEN_MIDI command (0xA0) turns port 0xA2 into a
 * stream of grouped records (see firmware/z80neo/include/midi.h):
 *
 *     [0] delay lo   } milliseconds before this record
 *     [1] delay hi   }
 *     [2] byte count 0 = delay only, 0xFF = end, else raw bytes follow
 *     [3..]          byte count raw MIDI message bytes, forwarded as-is
 *
 * Every channel message sharing a tick is coalesced into one record, so the
 * Z80 waits once per tick and then blits the tick's bytes, instead of paying
 * the per-record delay/console cost on every event.  This replaced an on-Z80
 * SMF parser: at 128 kHz the C parser cost 100-230 ms per event (~15x slow),
 * while streaming pre-parsed records costs only the port accesses.  The Pico
 * has the speed to do the parsing.
 *
 * Build:  make            -> build/midiplayer.ihx
 * Deploy: copy to the SD root and point Z80NEO.INI PROG1 at it.
 */

#include <stdint.h>
#include <stdio.h>

/* ---- Ports ------------------------------------------------------------- */

__sfr __at 0x80 CON_DATA;
__sfr __at 0x81 CON_STATUS;   /* bit0 = RX ready, bit1 = TX ready */
__sfr __at 0x90 MIDI_DATA;
__sfr __at 0x91 MIDI_STATUS;  /* bit0 = RX ready, bit1 = TX ready */
__sfr __at 0x92 MIDI_CFG_BAUD;
__sfr __at 0x93 MIDI_CFG_FMT;

__sfr __at 0xA0 FILE_CMD;
__sfr __at 0xA1 FILE_NAME;
__sfr __at 0xA2 FILE_DATA;
__sfr __at 0xA3 FILE_RST;

#define FCMD_CLOSE      2
#define FCMD_DIRRESET   5
#define FCMD_DIRNEXT    6
#define FCMD_OPEN_MIDI  8

#define FST_EOF     0x02
#define FST_ERR     0x04
#define FST_OPEN    0x08
#define FST_NAME    0x10

#define MIDI_BAUD_CODE 10   /* 31250 in the firmware table */
#define MIDI_FMT_8N1   0x03 /* data bits 8, stop 1, parity none */

/* Preferred directory on the SD card; falls back to the root if absent. */
#define MIDI_DIR "/MIDI"

/* Calibrated busy-wait (midi_delay.asm), ~128 kHz CPU clock. */
extern void midi_delay_ms(uint16_t ms);
/* Record replay loop (midi_delay.asm); returns on the 0xFF sentinel or quit. */
extern void midi_play(void);

/* ---- Small console helpers -------------------------------------------- */

static void con_put(uint8_t c)
{
	while ((CON_STATUS & 0x02) == 0)
		;
	CON_DATA = c;
}

static void con_puts(const char *s)
{
	while (*s)
		con_put((uint8_t)*s++);
}

static void con_num(uint16_t v)
{
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

static void con_hex8(uint8_t v)
{
	static const char hex[] = "0123456789ABCDEF";
	con_put((uint8_t)hex[v >> 4]);
	con_put((uint8_t)hex[v & 0x0F]);
}

/* Blocking line read; getchar() echoes through fputc_cons. */
static void read_line(char *buf, uint8_t max)
{
	uint8_t n = 0;
	int c;

	for (;;) {
		c = getchar();
		if (c == '\r' || c == '\n')
			break;
		if (c == 8 || c == 127) {
			if (n)
				n--;
			continue;
		}
		if (c >= 32 && n < (uint8_t)(max - 1))
			buf[n++] = (char)c;
	}
	buf[n] = '\0';
	con_puts("\r\n");
}

/* ---- MIDI out ---------------------------------------------------------- */

static void midi_init(void)
{
	/* Format first (with the current baud), then the MIDI baud code, so the
	 * final configure uses 31250 8N1. */
	MIDI_CFG_FMT = MIDI_FMT_8N1;
	MIDI_CFG_BAUD = MIDI_BAUD_CODE;
}

/* ---- Directory scan / file selection ----------------------------------- */

#define MAXF 16
#define NAME_MAX 40

static char    names[MAXF][NAME_MAX];
static uint8_t nfiles;
static char    g_dir[16];   /* directory in use, "" = root */

static uint8_t scan_files(const char *dir)
{
	uint8_t i, c;
	const char *p;

	nfiles = 0;
	FILE_RST = 0;
	for (p = dir; *p; p++)
		FILE_NAME = (uint8_t)*p;
	FILE_CMD = FCMD_DIRRESET;
	if (FILE_CMD & FST_ERR)
		return 1;

	while (nfiles < MAXF) {
		FILE_CMD = FCMD_DIRNEXT;
		if ((FILE_CMD & FST_NAME) == 0)
			break;
		i = 0;
		while ((c = FILE_NAME) != 0 && i < (NAME_MAX - 1))
			names[nfiles][i++] = (char)c;
		names[nfiles][i] = '\0';
		if (i)
			nfiles++;
	}
	return 0;
}

static void show_files(void)
{
	uint8_t i;

	if (nfiles == 0) {
		con_puts("No files in ");
		con_puts(g_dir[0] ? g_dir : "/");
		con_puts("\r\n");
		return;
	}
	con_puts("Files in ");
	con_puts(g_dir[0] ? g_dir : "/");
	con_puts(":\r\n");
	for (i = 0; i < nfiles; i++) {
		con_num((uint16_t)(i + 1));
		con_puts(") ");
		con_puts(names[i]);
		con_puts("\r\n");
	}
}

/* Build the full path for `name`: absolute if it starts with '/', otherwise
 * relative to g_dir ("" = root). */
static void make_path(char *out, uint8_t max, const char *name)
{
	uint8_t n = 0;
	const char *p;

	if (name[0] != '/' && g_dir[0]) {
		for (p = g_dir; *p && n < (uint8_t)(max - 1); p++)
			out[n++] = *p;
		if (n < (uint8_t)(max - 1))
			out[n++] = '/';
	}
	for (p = name; *p && n < (uint8_t)(max - 1); p++)
		out[n++] = *p;
	out[n] = '\0';
}

/* Ask for a number or a name; blank selects the first listed file.  Returns 0
 * if the user asked to quit ('q'), 1 when a filename was written to `out`. */
static uint8_t choose_file(char *out)
{
	char buf[NAME_MAX];
	uint8_t i, digits;
	uint16_t n;

	con_puts("File # or name (blank = first, q = quit): ");
	read_line(buf, sizeof buf);

	if (buf[0] == 'q' && buf[1] == '\0')
		return 0;
	if (buf[0] == 'Q' && buf[1] == '\0')
		return 0;

	if (buf[0] == '\0') {
		if (nfiles) {
			const char *p = names[0];
			while ((*out++ = *p++) != '\0')
				;
			return 1;
		}
		const char *d = "SONG.MID";
		while ((*out++ = *d++) != '\0')
			;
		return 1;
	}

	digits = 1;
	for (i = 0; buf[i]; i++)
		if (buf[i] < '0' || buf[i] > '9') { digits = 0; break; }
	if (digits) {
		n = 0;
		for (i = 0; buf[i]; i++)
			n = (uint16_t)(n * 10 + (uint16_t)(buf[i] - '0'));
		if (n >= 1 && n <= nfiles) {
			const char *p = names[n - 1];
			while ((*out++ = *p++) != '\0')
				;
			return 1;
		}
	}
	for (i = 0; buf[i]; i++)
		out[i] = buf[i];
	out[i] = '\0';
	return 1;
}

/* Pick the directory to list: /MIDI if it has files, else the card root.
 * Returns 1 and sets g_dir on success, 0 when no files are available. */
static uint8_t pick_dir(void)
{
	g_dir[0] = '\0';
	if (scan_files(MIDI_DIR) == 0 && nfiles > 0) {
		uint8_t i = 0;
		const char *p = MIDI_DIR;
		while (p[i] && i < (uint8_t)(sizeof g_dir - 1)) {
			g_dir[i] = p[i];
			i++;
		}
		g_dir[i] = '\0';
		return 1;
	}
	if (scan_files("") == 0 && nfiles > 0)
		return 1;
	return 0;
}

/* ---- main -------------------------------------------------------------- */

int main(void)
{
	char base[NAME_MAX];
	char path[NAME_MAX + 16];
	uint8_t s;

	con_puts("\r\nz80neo MIDI player\r\n");
	midi_init();
	con_puts("UART1 -> 31250 8N1 (MIDI); cfg code ");
	con_num((uint16_t)MIDI_CFG_BAUD);
	con_puts(MIDI_CFG_BAUD == MIDI_BAUD_CODE ? " ok" : "  (need firmware code 10)");
	con_puts("\r\n\r\n");

	/* Play one file, then list again so another can be chosen without a reset. */
	for (;;) {
		if (!pick_dir()) {
			con_puts("No MIDI files found in ");
			con_puts(MIDI_DIR);
			con_puts(" or the root\r\n");
			return 1;
		}

		show_files();
		con_puts("\r\n");
		if (!choose_file(base))
			break;
		make_path(path, sizeof path, base);

		con_puts("Opening ");
		con_puts(path);
		con_puts(" ... ");
		FILE_RST = 0;
		{
			const char *p = path;
			while (*p)
				FILE_NAME = (uint8_t)*p++;
		}
		FILE_CMD = FCMD_OPEN_MIDI;
		s = FILE_CMD;
		if ((s & FST_OPEN) == 0) {
			con_puts("failed (status 0x");
			con_hex8(s);
			con_puts(")\r\n\r\n");
			continue;
		}
		con_puts("ok\r\nPlaying (q or ESC to stop)...\r\n");

		midi_play();

		con_puts("\r\nStopped.\r\n\r\n");
		FILE_CMD = FCMD_CLOSE;
	}

	con_puts("Bye.\r\n");
	return 0;
}
