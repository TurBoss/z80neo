/*
 * monitor.c — z80neo machine monitor / debugger
 *
 * A standalone Z80 machine-language monitor, built for the in-tree z88dk
 * `+z80neo` target.  It links at 0x0000, owns page 0, and talks to the
 * console on ports 0x80 (data) / 0x81 (status).  Programs under test are
 * loaded into free page-0 RAM or into pages 1-3 (0x4000-0xFFFF) and driven
 * with breakpoints and single-stepping.
 *
 * Console access uses fgetc_cons()/fputc_cons(), which do not echo
 * themselves (the terminal's own echo is suppressed by the Pico firmware),
 * so this program owns all echoing and line editing.  Output uses the small
 * helpers below instead of printf, to keep the image inside page 0.
 */

#include <stdio.h>
#include <stdlib.h>

#include "monitor.h"

/* ---------------------------------------------------------------------------
 * Tunables / globals
 * ------------------------------------------------------------------------- */

#define LINE_MAX 80
#define MON_MAX_BP 8

unsigned int mon_regs[R_COUNT];

static char line[LINE_MAX];

static unsigned int dump_addr = 0;
static unsigned int free_base = 0;

static unsigned int bp_addr[MON_MAX_BP];
static unsigned char bp_used[MON_MAX_BP];
static unsigned char bp_orig[MON_MAX_BP];

/* ---------------------------------------------------------------------------
 * Memory / port access
 * ------------------------------------------------------------------------- */

unsigned char mon_peek(unsigned int addr) {
	return *(volatile unsigned char *)(addr & 0xFFFF);
}

void mon_poke(unsigned int addr, unsigned char value) {
	*(volatile unsigned char *)(addr & 0xFFFF) = value;
}

unsigned char mon_in(unsigned int port) {
	return (unsigned char)inp(port & 0xFFFF);
}

void mon_out(unsigned int port, unsigned char value) {
	outp(port & 0xFFFF, (unsigned int)value);
}

/* ---------------------------------------------------------------------------
 * Minimal output helpers
 * ------------------------------------------------------------------------- */

void mon_putstr(const char *s) {
	while (*s)
		fputc_cons(*s++);
}

void mon_puthex8(unsigned int v) {
	static const char *h = "0123456789ABCDEF";
	fputc_cons(h[(v >> 4) & 0x0F]);
	fputc_cons(h[v & 0x0F]);
}

void mon_puthex16(unsigned int v) {
	mon_puthex8((v >> 8) & 0xFF);
	mon_puthex8(v & 0xFF);
}

void mon_putdec(unsigned int v) {
	static const unsigned int pows[5] = {10000, 1000, 100, 10, 1};
	int started = 0;
	int i;

	if (v == 0) {
		fputc_cons('0');
		return;
	}
	for (i = 0; i < 5; i++) {
		unsigned int d = 0;
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

/* ---------------------------------------------------------------------------
 * Character / string helpers
 * ------------------------------------------------------------------------- */

static int upper(int c) {
	if (c >= 'a' && c <= 'z')
		return c - ('a' - 'A');
	return c;
}

static char *skip_ws(char *p) {
	while (*p == ' ' || *p == '\t')
		p++;
	return p;
}

static int hexval(int c) {
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

static int parse_hex_n(char **pp, int maxdigits, unsigned int *out) {
	char *p = skip_ws(*pp);
	unsigned int v = 0;
	int n = 0;
	int d;

	while ((d = hexval(*p)) >= 0) {
		v = ((v << 4) | (unsigned int)d) & 0xFFFF;
		p++;
		n++;
		if (n >= maxdigits)
			break;
	}
	if (n == 0)
		return 0;
	*pp = p;
	*out = v;
	return 1;
}

static int parse_word(char **pp, unsigned int *out) {
	return parse_hex_n(pp, 4, out);
}

static int parse_byte(char **pp, unsigned int *out) {
	return parse_hex_n(pp, 2, out);
}

static int readable(int c) { return c >= 0x20 && c < 0x7F; }

/* ---------------------------------------------------------------------------
 * Console line input (blocking, with local echo and line editing)
 * Returns the line length, or -1 if the edit was cancelled (ESC / Ctrl-C).
 * ------------------------------------------------------------------------- */

static int read_line(void) {
	int n = 0;
	int c;

	for (;;) {
		c = fgetc_cons();
		if (c == '\r' || c == '\n') {
			fputc_cons('\n');
			line[n] = 0;
			return n;
		}
		if (c == 0x1B || c == 0x03) { /* ESC or Ctrl-C */
			fputc_cons('\n');
			line[0] = 0;
			return -1;
		}
		if (c == 8 || c == 127) { /* backspace / DEL */
			if (n > 0) {
				n--;
				fputc_cons(8);
				fputc_cons(' ');
				fputc_cons(8);
			}
			continue;
		}
		if (c >= 0x20 && c < 0x7F) {
			if (n < LINE_MAX - 1) {
				line[n++] = (char)c;
				fputc_cons((char)c);
			}
		}
	}
}

/* Silent line read, used by the Intel HEX loader (no echo). */
static int read_line_silent(char *buf, int max) {
	int n = 0;
	int c;

	for (;;) {
		c = fgetc_cons();
		if (c == '\r' || c == '\n') {
			buf[n] = 0;
			return n;
		}
		if (c == 0x1B || c == 0x03) {
			buf[0] = 0;
			return -1;
		}
		if (c >= 0x20 && c < 0x7F) {
			if (n < max - 1)
				buf[n++] = (char)c;
		}
	}
}

/* ---------------------------------------------------------------------------
 * Breakpoints
 * ------------------------------------------------------------------------- */

static void bp_list(void) {
	int i;
	int any = 0;

	for (i = 0; i < MON_MAX_BP; i++) {
		if (bp_used[i]) {
			mon_putstr("  ");
			mon_putdec((unsigned int)i);
			mon_putstr(": ");
			mon_puthex16(bp_addr[i]);
			fputc_cons('\n');
			any = 1;
		}
	}
	if (!any)
		mon_putstr("no breakpoints\n");
}

static int bp_find(unsigned int addr) {
	int i;
	for (i = 0; i < MON_MAX_BP; i++)
		if (bp_used[i] && bp_addr[i] == addr)
			return i;
	return -1;
}

static void bp_add(unsigned int addr) {
	int i;

	if (bp_find(addr) >= 0) {
		mon_putstr("breakpoint already set at ");
		mon_puthex16(addr);
		fputc_cons('\n');
		return;
	}
	if (addr < free_base) {
		mon_putstr("address ");
		mon_puthex16(addr);
		mon_putstr(" is inside the monitor (free RAM starts at ");
		mon_puthex16(free_base);
		mon_putstr(")\n");
		return;
	}
	for (i = 0; i < MON_MAX_BP; i++) {
		if (!bp_used[i]) {
			bp_used[i] = 1;
			bp_addr[i] = addr;
			mon_putstr("breakpoint ");
			mon_putdec((unsigned int)i);
			mon_putstr(" at ");
			mon_puthex16(addr);
			fputc_cons('\n');
			return;
		}
	}
	mon_putstr("breakpoint table full\n");
}

static void bp_del(unsigned int addr) {
	int i = bp_find(addr);
	if (i < 0) {
		mon_putstr("no breakpoint at ");
		mon_puthex16(addr);
		fputc_cons('\n');
		return;
	}
	bp_used[i] = 0;
	mon_putstr("cleared breakpoint at ");
	mon_puthex16(addr);
	fputc_cons('\n');
}

static void bp_clear(void) {
	int i;
	for (i = 0; i < MON_MAX_BP; i++)
		bp_used[i] = 0;
	mon_putstr("all breakpoints cleared\n");
}

static void bp_patch_all(void) {
	int i;
	for (i = 0; i < MON_MAX_BP; i++) {
		if (bp_used[i]) {
			bp_orig[i] = mon_peek(bp_addr[i]);
			mon_poke(bp_addr[i], 0xFF); /* RST 38h */
		}
	}
}

static void bp_restore_all(void) {
	int i;
	for (i = 0; i < MON_MAX_BP; i++) {
		if (bp_used[i])
			mon_poke(bp_addr[i], bp_orig[i]);
	}
}

/* ---------------------------------------------------------------------------
 * Registers
 * ------------------------------------------------------------------------- */

static const char *reg_names[] = {"af",	 "bc",	"de", "hl", "af'", "bc'",
								  "de'", "hl'", "ix", "iy", "sp",  "pc"};

static void show_flags(unsigned int f) {
	static const char *names = "SZYHXPNC";
	int i;
	for (i = 0; i < 8; i++) {
		int bit = 7 - i;
		fputc_cons((char)((f >> bit) & 1 ? names[i] : '-'));
	}
}

static void show_regs(void) {
	unsigned int af = mon_regs[R_AF];
	unsigned int ir = mon_regs[R_IR];

	mon_putstr("AF =");
	mon_puthex16(af);
	mon_putstr("  BC =");
	mon_puthex16(mon_regs[R_BC]);
	mon_putstr("  DE =");
	mon_puthex16(mon_regs[R_DE]);
	mon_putstr("  HL =");
	mon_puthex16(mon_regs[R_HL]);
	fputc_cons('\n');

	mon_putstr("AF'=");
	mon_puthex16(mon_regs[R_AF2]);
	mon_putstr("  BC'=");
	mon_puthex16(mon_regs[R_BC2]);
	mon_putstr("  DE'=");
	mon_puthex16(mon_regs[R_DE2]);
	mon_putstr("  HL'=");
	mon_puthex16(mon_regs[R_HL2]);
	fputc_cons('\n');

	mon_putstr("IX =");
	mon_puthex16(mon_regs[R_IX]);
	mon_putstr("  IY =");
	mon_puthex16(mon_regs[R_IY]);
	mon_putstr("  SP =");
	mon_puthex16(mon_regs[R_SP]);
	mon_putstr("  PC =");
	mon_puthex16(mon_regs[R_PC]);
	fputc_cons('\n');

	mon_putstr("I  =");
	mon_puthex8(ir & 0xFF);
	mon_putstr("    R  =");
	mon_puthex8((ir >> 8) & 0xFF);
	mon_putstr("    F  =");
	show_flags(af & 0xFF);
	fputc_cons('\n');
}

static void set_register(char *p) {
	unsigned int value;
	int i;

	p = skip_ws(p);
	if (*p == 0) {
		show_regs();
		return;
	}

	if (upper(*p) == 'I' && (p[1] == 0 || p[1] == ' ')) {
		p = skip_ws(p + 1);
		if (!parse_byte(&p, &value)) {
			mon_putstr("?\n");
			return;
		}
		mon_regs[R_IR] = (mon_regs[R_IR] & 0xFF00) | (value & 0xFF);
		show_regs();
		return;
	}
	if (upper(*p) == 'R' && (p[1] == 0 || p[1] == ' ')) {
		p = skip_ws(p + 1);
		if (!parse_byte(&p, &value)) {
			mon_putstr("?\n");
			return;
		}
		mon_regs[R_IR] = (mon_regs[R_IR] & 0x00FF) | ((value & 0xFF) << 8);
		show_regs();
		return;
	}

	for (i = 0; i < 12; i++) {
		int n = 0;
		const char *name = reg_names[i];
		char *q = p;
		while (name[n] && q[n] && upper(q[n]) == upper(name[n]))
			n++;
		if (name[n] == 0 && (q[n] == 0 || q[n] == ' ')) {
			q = skip_ws(q + n);
			if (!parse_word(&q, &value)) {
				mon_putstr("?\n");
				return;
			}
			mon_regs[i] = value & 0xFFFF;
			show_regs();
			return;
		}
	}
	mon_putstr("unknown register: ");
	mon_putstr(p);
	fputc_cons('\n');
}

/* ---------------------------------------------------------------------------
 * Run / step
 * ------------------------------------------------------------------------- */

static void run_user(void) {
	bp_patch_all();
	mon_run();
	bp_restore_all();
	mon_putstr("\n*** break at ");
	mon_puthex16(mon_regs[R_PC]);
	fputc_cons('\n');
	show_regs();
}

static void step_user(unsigned int count) {
	unsigned int pc;
	unsigned int next;
	unsigned int orig;
	unsigned int done = 0;

	if (count == 0)
		count = 1;

	while (count-- > 0) {
		pc = mon_regs[R_PC];
		next = (pc + mon_instr_len(pc)) & 0xFFFF;

		bp_patch_all();
		orig = mon_peek(next);
		mon_poke(next, 0xFF);
		mon_run();
		mon_poke(next, orig);
		bp_restore_all();
		done++;

		if (mon_regs[R_PC] != next) {
			mon_putstr("\n*** stopped at ");
			mon_puthex16(mon_regs[R_PC]);
			fputc_cons('\n');
			show_regs();
			return;
		}
	}
	mon_putstr("\n*** stepped ");
	mon_putdec(done);
	fputc_cons('\n');
	show_regs();
}

/* ---------------------------------------------------------------------------
 * Memory commands
 * ------------------------------------------------------------------------- */

static void dump_line(unsigned int addr) {
	int i;

	mon_puthex16(addr);
	mon_putstr(": ");
	for (i = 0; i < 16; i++) {
		mon_puthex8(mon_peek((addr + i) & 0xFFFF));
		fputc_cons(' ');
	}
	fputc_cons('|');
	for (i = 0; i < 16; i++) {
		int c = mon_peek((addr + i) & 0xFFFF);
		fputc_cons((char)(readable(c) ? c : '.'));
	}
	fputc_cons('|');
	fputc_cons('\n');
}

static void cmd_dump(char *p) {
	unsigned int start;
	unsigned int end;
	int have_end = 0;

	if (parse_word(&p, &start)) {
		dump_addr = start & 0xFFFF;
		if (parse_word(&p, &end)) {
			end &= 0xFFFF;
			have_end = 1;
		}
	} else {
		start = dump_addr;
	}
	start &= 0xFFFF;
	if (!have_end)
		end = (start + 127) & 0xFFFF;
	if (end < start)
		end = start;

	for (;;) {
		dump_line(start);
		if (start == end)
			break;
		if ((start & 0xFFF0) == (end & 0xFFF0))
			break;
		start = (start + 16) & 0xFFFF;
	}
	dump_addr = (end + 1) & 0xFFFF;
}

static void cmd_examine(char *p) {
	unsigned int addr;

	if (!parse_word(&p, &addr)) {
		mon_putstr("usage: E addr\n");
		return;
	}
	addr &= 0xFFFF;

	for (;;) {
		unsigned int v;
		char *q;

		mon_puthex16(addr);
		mon_putstr(": ");
		mon_puthex8(mon_peek(addr));
		mon_putstr("  ");
		if (read_line() < 0)
			return;
		q = skip_ws(line);
		if (*q == 0) {
			addr = (addr + 1) & 0xFFFF;
			continue;
		}
		if (*q == '.')
			return;
		if (*q == '-') {
			addr = (addr - 1) & 0xFFFF;
			continue;
		}
		if (parse_byte(&q, &v)) {
			mon_poke(addr, (unsigned char)v);
			addr = (addr + 1) & 0xFFFF;
		} else {
			mon_putstr("?\n");
		}
	}
}

static void cmd_fill(char *p) {
	unsigned int start, end, val, a;

	if (!parse_word(&p, &start) || !parse_word(&p, &end) ||
		!parse_byte(&p, &val)) {
		mon_putstr("usage: F start end byte\n");
		return;
	}
	start &= 0xFFFF;
	end &= 0xFFFF;
	val &= 0xFF;

	a = start;
	for (;;) {
		mon_poke(a, (unsigned char)val);
		if (a == end)
			break;
		a = (a + 1) & 0xFFFF;
	}
	mon_putstr("filled ");
	mon_puthex16(start);
	mon_putstr("..");
	mon_puthex16(end);
	mon_putstr(" with ");
	mon_puthex8(val);
	fputc_cons('\n');
}

static void cmd_move(char *p) {
	unsigned int start, end, dest;
	unsigned int count;
	unsigned int i;

	if (!parse_word(&p, &start) || !parse_word(&p, &end) ||
		!parse_word(&p, &dest)) {
		mon_putstr("usage: M start end dest\n");
		return;
	}
	start &= 0xFFFF;
	end &= 0xFFFF;
	dest &= 0xFFFF;
	count = ((end - start) & 0xFFFF) + 1;

	if (dest > start && dest <= end) {
		i = count;
		while (i-- > 0) {
			unsigned int from = (start + i) & 0xFFFF;
			mon_poke((dest + i) & 0xFFFF, mon_peek(from));
		}
	} else {
		for (i = 0; i < count; i++)
			mon_poke((dest + i) & 0xFFFF, mon_peek((start + i) & 0xFFFF));
	}
	mon_putstr("moved ");
	mon_putdec(count);
	mon_putstr(" bytes to ");
	mon_puthex16(dest);
	fputc_cons('\n');
}

static void cmd_compare(char *p) {
	unsigned int start, end, dest, a;
	unsigned int diffs = 0;

	if (!parse_word(&p, &start) || !parse_word(&p, &end) ||
		!parse_word(&p, &dest)) {
		mon_putstr("usage: C start end dest\n");
		return;
	}
	start &= 0xFFFF;
	end &= 0xFFFF;
	dest &= 0xFFFF;

	a = start;
	for (;;) {
		unsigned char x = mon_peek(a);
		unsigned char y = mon_peek(dest);
		if (x != y) {
			mon_puthex16(a);
			mon_putstr(": ");
			mon_puthex8(x);
			mon_putstr("  ");
			mon_puthex16(dest);
			mon_putstr(": ");
			mon_puthex8(y);
			fputc_cons('\n');
			diffs++;
		}
		if (a == end)
			break;
		a = (a + 1) & 0xFFFF;
		dest = (dest + 1) & 0xFFFF;
	}
	mon_putdec(diffs);
	mon_putstr(" difference(s)\n");
}

/* ---------------------------------------------------------------------------
 * I/O and MMU
 * ------------------------------------------------------------------------- */

static void cmd_in(char *p) {
	unsigned int port;
	if (!parse_word(&p, &port)) {
		mon_putstr("usage: I port\n");
		return;
	}
	mon_putstr("port ");
	mon_puthex8(port & 0xFF);
	mon_putstr(" = ");
	mon_puthex8(mon_in(port));
	fputc_cons('\n');
}

static void cmd_out(char *p) {
	unsigned int port, val;
	if (!parse_word(&p, &port) || !parse_byte(&p, &val)) {
		mon_putstr("usage: O port byte\n");
		return;
	}
	mon_out(port, (unsigned char)val);
	mon_putstr("port ");
	mon_puthex8(port & 0xFF);
	mon_putstr(" <- ");
	mon_puthex8(val);
	fputc_cons('\n');
}

static void cmd_mmu(char *p) {
	unsigned int v;

	p = skip_ws(p);
	if (*p == 0) {
		mon_putstr("MMU pages: 0=");
		mon_puthex8(mon_in(0xF0));
		mon_putstr(" 1=");
		mon_puthex8(mon_in(0xF1));
		mon_putstr(" 2=");
		mon_puthex8(mon_in(0xF2));
		mon_putstr(" 3=");
		mon_puthex8(mon_in(0xF3));
		fputc_cons('\n');
		return;
	}

	if (!parse_word(&p, &v)) {
		mon_putstr("usage: P [p1 [p2 [p3]]]\n");
		return;
	}
	mon_out(0xF1, (unsigned char)v);
	if (parse_word(&p, &v))
		mon_out(0xF2, (unsigned char)v);
	if (parse_word(&p, &v))
		mon_out(0xF3, (unsigned char)v);
	mon_putstr("MMU pages: 0=");
	mon_puthex8(mon_in(0xF0));
	mon_putstr(" 1=");
	mon_puthex8(mon_in(0xF1));
	mon_putstr(" 2=");
	mon_puthex8(mon_in(0xF2));
	mon_putstr(" 3=");
	mon_puthex8(mon_in(0xF3));
	fputc_cons('\n');
}

/* ---------------------------------------------------------------------------
 * Intel HEX loader
 * ------------------------------------------------------------------------- */

static void cmd_load(void) {
	char buf[80];
	unsigned int base = 0;
	unsigned int total = 0;
	unsigned int refused = 0;

	mon_putstr("Send Intel HEX, blank line or EOF record to finish\n");
	mon_putstr("(monitor occupies 0x0000..");
	mon_puthex16(free_base - 1);
	mon_putstr(")\n");

	for (;;) {
		int len;
		unsigned int reclen, addr, type, sum, csum, i;
		char *p;
		unsigned int bytes[32];

		if (read_line_silent(buf, sizeof(buf)) < 0)
			break;
		len = 0;
		while (buf[len])
			len++;
		if (len == 0)
			break;
		if (buf[0] != ':')
			continue;

		p = buf + 1;
		if (!parse_hex_n(&p, 2, &reclen) || !parse_hex_n(&p, 4, &addr) ||
			!parse_hex_n(&p, 2, &type))
			continue;
		if (reclen > 32) {
			mon_putstr("record too long, skipped\n");
			continue;
		}

		sum = (reclen + ((addr >> 8) & 0xFF) + (addr & 0xFF) + type) & 0xFF;
		for (i = 0; i < reclen; i++) {
			unsigned int b;
			if (!parse_hex_n(&p, 2, &b))
				break;
			bytes[i] = b;
			sum = (sum + b) & 0xFF;
		}
		if (!parse_hex_n(&p, 2, &csum))
			continue;
		if (((sum + csum) & 0xFF) != 0) {
			mon_putstr("checksum error at ");
			mon_puthex16((base + addr) & 0xFFFF);
			fputc_cons('\n');
			continue;
		}

		if (type == 1)
			break;
		if (type == 2 && reclen >= 2) {
			base = (((bytes[0] << 8) | bytes[1]) << 4) & 0xFFFF;
			continue;
		}
		if (type == 4 && reclen >= 2)
			continue;
		if (type != 0)
			continue;

		for (i = 0; i < reclen; i++) {
			unsigned int target = (base + addr + i) & 0xFFFF;
			if (target < free_base) {
				refused++;
				continue;
			}
			mon_poke(target, (unsigned char)bytes[i]);
			total++;
		}
		if ((total & 0x00FF) == 0)
			fputc_cons('.');
	}
	fputc_cons('\n');
	mon_putstr("loaded ");
	mon_putdec(total);
	mon_putstr(" byte(s)\n");
	if (refused) {
		mon_putstr("warning: ");
		mon_putdec(refused);
		mon_putstr(" byte(s) below free RAM were refused\n");
	}
}

/* ---------------------------------------------------------------------------
 * Help
 * ------------------------------------------------------------------------- */

static void cmd_help(void) {
	mon_putstr(
		"Commands:\n"
		"H|? help    D [s[e]] dump     E addr examine    F s e b fill\n"
		"M s e d move   C s e d compare   U [addr] disassemble\n"
		"R [reg val] registers (af bc de hl af' bc' de' hl' ix iy sp pc i r)\n"
		"G [addr] run   T [n] step\n"
		"B list   B addr set   B -addr clear   B * clear-all\n"
		"I port in   O port byte out   P [p1 p2 p3] MMU pages\n"
		"L load Intel HEX from the console\n");
	mon_putstr("free RAM from ");
	mon_puthex16(free_base);
	mon_putstr("; programs normally load at 0x4000+\n");
}

/* ---------------------------------------------------------------------------
 * Command dispatcher
 * ------------------------------------------------------------------------- */

static void dispatch(void) {
	char *p = skip_ws(line);
	char c;

	if (*p == 0)
		return;
	c = (char)upper(*p);
	p++;

	switch (c) {
	case '?':
	case 'H':
		cmd_help();
		break;
	case 'D':
		cmd_dump(p);
		break;
	case 'E':
		cmd_examine(p);
		break;
	case 'F':
		cmd_fill(p);
		break;
	case 'M':
		cmd_move(p);
		break;
	case 'C':
		cmd_compare(p);
		break;
	case 'U':
		cmd_disasm(p);
		break;
	case 'R':
		set_register(p);
		break;
	case 'G': {
		unsigned int addr;
		if (parse_word(&p, &addr))
			mon_regs[R_PC] = addr & 0xFFFF;
		run_user();
		break;
	}
	case 'T': {
		unsigned int n;
		if (!parse_word(&p, &n))
			n = 1;
		step_user(n);
		break;
	}
	case 'B':
		p = skip_ws(p);
		if (*p == 0) {
			bp_list();
		} else if (*p == '*') {
			bp_clear();
		} else if (*p == '-') {
			unsigned int addr;
			p++;
			if (parse_word(&p, &addr))
				bp_del(addr & 0xFFFF);
			else
				mon_putstr("usage: B -addr\n");
		} else {
			unsigned int addr;
			if (parse_word(&p, &addr))
				bp_add(addr & 0xFFFF);
			else
				mon_putstr("usage: B [addr|-addr|*]\n");
		}
		break;
	case 'X':
		bp_clear();
		break;
	case 'I':
		cmd_in(p);
		break;
	case 'O':
		cmd_out(p);
		break;
	case 'P':
		cmd_mmu(p);
		break;
	case 'L':
		cmd_load();
		break;
	default:
		mon_putstr("?\n");
		break;
	}
}

/* ---------------------------------------------------------------------------
 * Monitor entry / main loop
 * ------------------------------------------------------------------------- */

void monitor_body(void) {
	free_base = mon_mem_top();

	mon_regs[R_SP] =
		0x7FFF; /* user stack: top of page 1, clear of the monitor */

	mon_putstr("\nz80neo monitor\n");
	mon_putstr("free RAM 0x");
	mon_puthex16(free_base);
	mon_putstr("-0x3FFF, then 0x4000-0xFFFF (MMU pages 1-3)\n");
	mon_putstr("type H for help\n");

	for (;;) {
		mon_putstr("z80neo> ");
		if (read_line() < 0)
			continue;
		dispatch();
	}
}
