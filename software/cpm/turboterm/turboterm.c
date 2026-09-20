/*
 * turboterm.c — CP/M console <-> UART1 terminal client for the z80neo.
 *
 * A CP/M .COM that bridges the z80neo console (the VDP or the PC sdl_client)
 * to the second UART the firmware exposes at ports 0x90 (data) / 0x91 (status:
 * bit0 = RX ready, bit1 = TX ready, hardware UART1 on GPIO36/37).  Anything
 * typed at the CP/M prompt is sent to UART1 and anything received from UART1 is
 * printed to the console, so a device on UART1 can be driven interactively.
 *
 * UART1 line settings can be changed from the command line; they are written to
 * the firmware config ports 0x92 (baud code) / 0x93 (format byte), which retune
 * the hardware UART1 for every program until reset.  With no arguments the
 * current settings (the Z80NEO.INI defaults) are left alone and shown.
 *
 *   TTERM [-b BAUD] [-f FMT] [-?]
 *     -b 19200        baud: 115200 57600 38400 19200 9600 4800 2400 1200 600 300
 *     -f 8N1          data bits 5..8, parity N/E/O, stop bits 1/2
 *     -?              show help
 *   A bare number is taken as the baud, a bare dP s (e.g. 7E1) as the format.
 *
 * Exit back to CP/M with Ctrl-] (0x1D).
 *
 * Built with the in-tree z88dk's +cpm target; see the Makefile.
 */

#include <cpm.h>
#include <stdint.h>

__sfr __at 0x90 UART1_DATA;
__sfr __at 0x91 UART1_STATUS;   // bit0 = RX ready, bit1 = TX ready
__sfr __at 0x92 UART1_CFG_BAUD; // write: baud code; read: current code
__sfr __at 0x93 UART1_CFG_FMT;  // write: format byte; read: current format

#define KEY_EXIT   0x1D         // Ctrl-]
#define CMD_TAIL   0x0080

#define PAR_NONE 0
#define PAR_EVEN 1
#define PAR_ODD  2

static const unsigned long baud_codes[] = {
	115200, 57600, 38400, 19200, 9600, 4800, 2400, 1200, 600, 300
};
static const char *const baud_names[] = {
	"115200", "57600", "38400", "19200", "9600", "4800", "2400", "1200", "600", "300"
};
#define BAUD_CODE_COUNT (sizeof(baud_codes) / sizeof(baud_codes[0]))

static char argbuf[128];

static void con_put(uint8_t c)
{
	bdos(CPM_DCIO, c);
}

static void con_puts(const char *s)
{
	while (*s)
		con_put((uint8_t)*s++);
}

/* Non-blocking console read; returns 0 when no key is waiting. */
static int con_get(void)
{
	return bdos(CPM_DCIO, 0xFF);
}

/* Returns 1 if the byte was queued, 0 on timeout (TX never became ready). */
static int uart1_put(uint8_t c)
{
	unsigned n = 0xFFFF;

	while ((UART1_STATUS & 0x02) == 0)
		if (--n == 0)
			return 0;
	UART1_DATA = c;
	return 1;
}

static int parse_u32(const char *s, unsigned long *out)
{
	unsigned long v = 0;

	if (*s == '\0')
		return 0;
	while (*s) {
		if (*s < '0' || *s > '9')
			return 0;
		v = v * 10 + (unsigned long)(*s - '0');
		if (v > 3000000)
			return 0;
		s++;
	}
	*out = v;
	return 1;
}

/* "8N1": data bits 5..8, parity N/E/O, stop bits 1/2. */
static int parse_fmt(const char *s, uint8_t *data_bits, uint8_t *parity, uint8_t *stop_bits)
{
	uint8_t p;

	if (s[0] < '5' || s[0] > '8')
		return 0;
	if (s[1] == 'N' || s[1] == 'n') p = PAR_NONE;
	else if (s[1] == 'E' || s[1] == 'e') p = PAR_EVEN;
	else if (s[1] == 'O' || s[1] == 'o') p = PAR_ODD;
	else return 0;
	if ((s[2] != '1' && s[2] != '2') || s[3] != '\0')
		return 0;
	*data_bits = (uint8_t)(s[0] - '0');
	*parity    = p;
	*stop_bits = (uint8_t)(s[2] - '0');
	return 1;
}

static int baud_code(unsigned long baud, uint8_t *code)
{
	uint8_t i;

	for (i = 0; i < BAUD_CODE_COUNT; i++) {
		if (baud_codes[i] == baud) {
			*code = i;
			return 1;
		}
	}
	return 0;
}

/* Split on spaces or NULs (the +cpm crt0 already turned spaces into NULs). */
static char *next_tok(char **p, char *end)
{
	char *s = *p;
	char *start;

	while (s < end && (*s == ' ' || *s == '\t' || *s == '\0'))
		s++;
	if (s >= end) {
		*p = s;
		return NULL;
	}
	start = s;
	while (s < end && *s != ' ' && *s != '\t' && *s != '\0')
		s++;
	if (s < end)
		*s++ = '\0';
	*p = s;
	return start;
}

static void usage(void)
{
	con_puts("\r\nTTERM [-b BAUD] [-f FMT] [-?]\r\n"
	         "  -b  baud: 115200 57600 38400 19200 9600 4800 2400 1200 600 300\r\n"
	         "  -f  line format, e.g. 8N1 or 7E2 (data 5-8, parity N/E/O, stop 1-2)\r\n"
	         "  -?  this help\r\n"
	         "  bare \"19200\" or \"8N1\" also accepted.  Ctrl-] exits.\r\n\r\n");
}

static void show_settings(void)
{
	uint8_t fmt = UART1_CFG_FMT;
	uint8_t code = UART1_CFG_BAUD;
	uint8_t db, par, sb;
	char pc;

	if (!(fmt & 0x80))
		fmt = 0x03;             // no config port: hardware default is 8N1
	fmt &= 0x3F;
	db = (uint8_t)(5 + (fmt & 0x03));
	par = (fmt >> 4) & 0x03;
	sb = (fmt & 0x08) ? 2 : 1;
	pc = (par == PAR_EVEN) ? 'E' : (par == PAR_ODD) ? 'O' : 'N';

	con_puts("UART1: ");
	con_puts(code < BAUD_CODE_COUNT ? baud_names[code] : "?");
	con_put(' ');
	con_put((uint8_t)('0' + db));
	con_put((uint8_t)pc);
	con_put((uint8_t)('0' + sb));
	con_puts("\r\n");
}

int main(void)
{
	unsigned char *tail = (unsigned char *)CMD_TAIL;
	unsigned n = tail[0];
	unsigned long want_baud = 0;
	uint8_t db = 0, par = PAR_NONE, sb = 0;
	int have_fmt = 0;
	int help = 0;
	int bad = 0;
	char *p, *t, *end;
	unsigned i;

	if (n > sizeof(argbuf) - 1)
		n = sizeof(argbuf) - 1;
	for (i = 0; i < n; i++)
		argbuf[i] = (char)tail[1 + i];
	argbuf[n] = '\0';

	p = argbuf;
	end = argbuf + n;
	while ((t = next_tok(&p, end)) != NULL) {
		if (*t == '-' || *t == '/') {
			char f = t[1];
			char *val = t + 2;

			if (f >= 'a' && f <= 'z')
				f = (char)(f - 32);
			if (f == '?' || f == 'H') {
				help = 1;
				continue;
			}
			if (f == 'B') {
				if (*val == '\0')
					val = next_tok(&p, end);
				if (val == NULL || !parse_u32(val, &want_baud))
					bad = 1;
				continue;
			}
			if (f == 'F') {
				if (*val == '\0')
					val = next_tok(&p, end);
				if (val == NULL || !parse_fmt(val, &db, &par, &sb))
					bad = 1;
				else
					have_fmt = 1;
				continue;
			}
			bad = 1;
		} else if (parse_u32(t, &want_baud)) {
			continue;
		} else if (parse_fmt(t, &db, &par, &sb)) {
			have_fmt = 1;
		} else {
			bad = 1;
		}
	}

	if (help) {
		usage();
		return 0;
	}
	if (bad) {
		con_puts("\r\nTTERM: bad argument.\r\n");
		usage();
		return 1;
	}

	if (want_baud != 0) {
		uint8_t code;
		if (!baud_code(want_baud, &code)) {
			con_puts("\r\nTTERM: unsupported baud.\r\n");
			usage();
			return 1;
		}
		UART1_CFG_BAUD = code;
	}
	if (have_fmt)
		UART1_CFG_FMT = (uint8_t)(((db - 5) & 0x03) |
		                          ((sb == 2) ? 0x08 : 0x00) |
		                          ((par & 0x03) << 4));

	while (UART1_STATUS & 0x01)     // drop anything buffered before we start
		(void)UART1_DATA;

	con_puts("\r\nturboterm - CP/M console <-> UART1 (0x90/0x91)\r\n");
	show_settings();
	con_puts("Ctrl-] exits back to CP/M.\r\n\r\n");

	for (;;) {
		int c = con_get();

		if (c == KEY_EXIT)
			break;
		if (c)
			uart1_put((uint8_t)c);

		if (UART1_STATUS & 0x01)
			con_put(UART1_DATA);
	}

	con_puts("\r\n[turboterm exit]\r\n");
	return 0;
}
