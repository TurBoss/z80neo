/*
 * loader.c — z80neo serial bootloader
 *
 * Built with the in-tree z88dk `+z80neo` target.  It links at 0x0000, owns
 * page 0 and talks to the console on ports 0x80 (data) / 0x81 (status).
 *
 * It receives a program over the console UART as Intel HEX records (or the
 * simple `@addr byte...` form), loads it into Z80 memory and jumps to the
 * entry point.  Records are paced one line at a time: after every line the
 * loader emits a single '.' so the host knows it may send the next one.  The
 * NMOS Z80 at 128 kHz is far slower than 115200 baud, so unpaced bulk sends
 * would overflow the firmware's 256-byte console ring.
 *
 * Two load paths:
 *   - image at 0x4000..0xFFFF: written in place, then entered with SP=0xFFFF.
 *   - image at 0x0000..0x3FFF: staged byte-for-byte at 0x4000+addr, then a
 *     position-independent copier running from page 2 (0x8000) copies it down
 *     to page 0 and enters it.  The copier first remaps MMU page 0 to 0x34
 *     (which aliases bank 0, the normal default) so the firmware's reset-vector
 *     write-protect — which matches the register value 0x20 exactly — does not
 *     silently drop the byte at address 0x0000.
 */

#include <stdio.h>

typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned long u32;

#define CON_DATA 0x80
#define CON_STAT 0x81

#define PAGE0_TOP 0x4000u
#define STAGE_BASE 0x4000u
#define IMG_SPAN 0x4000u

#define LINE_MAX 560 /* 255-byte Intel record + overhead */
#define REC_MAX 256

/* Filled by the loader, consumed by loader_asm.asm before the jump. */
unsigned int boot_entry, boot_sp;
unsigned int rel_dst, rel_src, rel_len, rel_entry, rel_sp;

void loader_start(void);
void loader_relocate(void);

extern unsigned int inp(unsigned int port);
extern void outp(unsigned int port, unsigned int value);

/* --------------------------------------------------------------------------
 * Console
 * ------------------------------------------------------------------------ */

static void putc_(char c) { fputc_cons(c); }

static void puts_(const char *s) {
	while (*s)
		fputc_cons(*s++);
}

static void puthex8(u16 v) {
	static const char *h = "0123456789ABCDEF";
	fputc_cons(h[(v >> 4) & 0x0F]);
	fputc_cons(h[v & 0x0F]);
}

static void puthex16(u16 v) {
	puthex8((v >> 8) & 0xFF);
	puthex8(v & 0xFF);
}

static void putdec(u16 v) {
	const u16 pows[5] = {10000, 1000, 100, 10, 1};
	int started = 0;
	int i;

	if (v == 0) {
		fputc_cons('0');
		return;
	}
	for (i = 0; i < 5; i++) {
		u16 d = 0;
		while (v >= pows[i]) {
			v -= pows[i];
			d++;
		}
		if (d || started) {
			fputc_cons((char)('0' + d));
			started = 1;
		}
	}
}

/* Read one byte, blocking on the console status port. */
static int con_get(void) {
	while (!(inp(CON_STAT) & 0x01))
		;
	return (int)(inp(CON_DATA) & 0xFF);
}

/* --------------------------------------------------------------------------
 * Transmission state
 * ------------------------------------------------------------------------ */

static char line[LINE_MAX];
static u8 rec[REC_MAX];

static u16 base;
static u16 lo, hi;
static u16 start;
static u16 chk_addr;
static int have_start;
static u16 total;
static int mode; /* 0 = none, 1 = direct (>=0x4000), 2 = staged (<0x4000) */
static int mixed;
static int stage_zeroed;

static void reset_state(void) {
	base = 0;
	lo = 0xFFFF;
	hi = 0;
	start = 0;
	chk_addr = 0;
	have_start = 0;
	total = 0;
	mode = 0;
	mixed = 0;
}

/* The staging window aliases the loader image copied into bank 1 at boot, so
 * clear it before the first staged byte; gaps must read back as zero. */
static void zero_stage(void) {
	u16 i;
	for (i = 0; i < (u16)IMG_SPAN; i++)
		((volatile u8 *)STAGE_BASE)[i] = 0;
}

/* Write one loaded byte to the target, choosing the load path on first use. */
static void put_byte(u16 addr, u8 val) {
	total++;

	if (addr < PAGE0_TOP) {
		if (mode == 1)
			mixed = 1;
		if (mode != 2 && !stage_zeroed) {
			zero_stage();
			stage_zeroed = 1;
		}
		mode = 2;
		if (addr < lo)
			lo = addr;
		if (addr > hi)
			hi = addr;
		*(volatile u8 *)(STAGE_BASE + addr) = val;
	} else {
		if (mode == 2)
			mixed = 1;
		mode = 1;
		if (addr < lo)
			lo = addr;
		if (addr > hi)
			hi = addr;
		*(volatile u8 *)addr = val;
	}
}

/* --------------------------------------------------------------------------
 * Line input (silent, CR/LF tolerant)
 * ------------------------------------------------------------------------ */

static int pending = -1;

static int rd(void) {
	if (pending >= 0) {
		int c = pending;
		pending = -1;
		return c;
	}
	return con_get();
}

static int read_line(char *buf, int max) {
	int n = 0;
	int c;

	for (;;) {
		c = rd();
		if (c == '\r') {
			c = rd();
			if (c != '\n')
				pending = c;
			break;
		}
		if (c == '\n')
			break;
		if (c == 0x1B || c == 0x03)
			return -1;
		if (n < max - 1)
			buf[n++] = (char)c;
	}
	buf[n] = 0;
	return n;
}

/* --------------------------------------------------------------------------
 * Record parsing
 * ------------------------------------------------------------------------ */

static int hexv(int c) {
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

static int hex_byte(const char **pp, u8 *out) {
	const char *p = *pp;
	int h = hexv((u8)p[0]);
	int l;

	if (h < 0)
		return 0;
	l = hexv((u8)p[1]);
	if (l < 0)
		return 0;
	*out = (u8)((h << 4) | l);
	*pp = p + 2;
	return 1;
}

/* Intel HEX.  Returns 1 = data/control ok, 2 = EOF record, -1 = checksum. */
static int handle_intel(const char *s) {
	const char *p = s + 1;
	u8 count, ah, al, type, sum, b;
	u16 addr, i;

	if (!hex_byte(&p, &count) || !hex_byte(&p, &ah) || !hex_byte(&p, &al) ||
		!hex_byte(&p, &type))
		return 1;
	sum = (u8)(count + ah + al + type);
	addr = (u16)(((u16)ah << 8) | al);

	for (i = 0; i < count; i++) {
		if (!hex_byte(&p, &b))
			return 1;
		rec[i] = b;
		sum = (u8)(sum + b);
	}
	if (!hex_byte(&p, &b))
		return 1;
	sum = (u8)(sum + b);
	if (sum != 0) {
		chk_addr = addr;
		return -1;
	}

	switch (type) {
	case 0:
		for (i = 0; i < count; i++)
			put_byte((u16)((base + addr + i) & 0xFFFF), rec[i]);
		return 1;
	case 1:
		return 2;
	case 2:
		if (count >= 2)
			base = (u16)((((u16)rec[0] << 8) | rec[1]) << 4);
		return 1;
	case 3:
	case 5:
		if (count >= 2) {
			start = (u16)(((u16)rec[0] << 8) | rec[1]);
			have_start = 1;
		}
		return 1;
	case 4:
		/* 16-bit address space: the linear base only matters if 0. */
		return 1;
	default:
		return 1;
	}
}

/* `@AAAAAAAA BB BB ...` — address followed by whitespace-separated bytes. */
static int handle_at(const char *s) {
	const char *p = s + 1;
	u32 a = 0;
	int n = 0;
	u16 addr;

	while (hexv((u8)*p) >= 0) {
		a = (a << 4) | (u32)hexv((u8)*p);
		p++;
		if (++n > 8)
			break;
	}
	if (n == 0)
		return 1;
	addr = (u16)(a & 0xFFFF);

	for (;;) {
		int h, l;
		while (*p == ' ' || *p == '\t')
			p++;
		if (!*p)
			break;
		h = hexv((u8)*p);
		l = hexv((u8)p[1]);
		if (h < 0 || l < 0)
			break;
		put_byte(addr++, (u8)((h << 4) | l));
		p += 2;
	}
	return 1;
}

/* --------------------------------------------------------------------------
 * Receive / run
 * ------------------------------------------------------------------------ */

/* Read one transmission.  Returns 1 when an image is ready to run. */
static int receive(void) {
	reset_state();

	puts_("ld> ");

	for (;;) {
		int n = read_line(line, LINE_MAX);
		int st;

		if (n < 0)
			return 0;
		if (n == 0)
			break;

		if (line[0] == ':')
			st = handle_intel(line);
		else if (line[0] == '@')
			st = handle_at(line);
		else {
			putc_('.');
			continue;
		}

		if (st < 0) {
			puts_("\n!chk ");
			puthex16(chk_addr);
			puts_("\n");
		}
		putc_('.');
		if (st == 2)
			break;
	}

	if (mode == 0) {
		puts_("\nno data\n");
		return 0;
	}
	if (mixed) {
		puts_("\n!mixed 0x0000/0x4000 image not supported\n");
		return 0;
	}
	return 1;
}

static void run_image(void) {
	u16 entry = have_start ? start : lo;

	puts_("\nloaded ");
	putdec(total);
	puts_(" bytes, run @");
	puthex16(entry);
	putc_('\n');

	if (mode == 1) {
		boot_entry = entry;
		boot_sp = 0xFFFF;
		loader_start();
	} else {
		rel_dst = lo;
		rel_src = (u16)(STAGE_BASE + lo);
		rel_len = (u16)(hi - lo + 1);
		rel_entry = entry;
		rel_sp = 0x3FFF;
		loader_relocate();
	}
	for (;;)
		;
}

void main(void) {
	puts_("\nz80neo serial loader\n");
	puts_("send Intel HEX or @addr records; EOF/blank runs\n");

	for (;;) {
		if (receive())
			run_image();
	}
}
