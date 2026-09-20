/*
 * disasm.c — Z80 instruction length + disassembler for the z80neo monitor.
 *
 * mon_instr_len() is also what makes single-stepping work: the monitor places
 * a temporary RST 38h at PC + length and runs until it traps there.
 */

#include <stdio.h>

#include "monitor.h"

static const char *r8_name[8] = {"B", "C", "D", "E", "H", "L", "(HL)", "A"};
static const char *rp_name[4] = {"BC", "DE", "HL", "SP"};
static const char *rp2_name[4] = {"BC", "DE", "HL", "AF"};
static const char *cc_name[8] = {"NZ", "Z", "NC", "C", "PO", "PE", "P", "M"};
static const char *alu_name[8] = {"ADD A,", "ADC A,", "SUB ", "SBC A,",
								  "AND ",	"XOR ",	  "OR ",  "CP "};
static const char *rot_name[8] = {"RLC", "RRC", "RL",  "RR",
								  "SLA", "SRA", "SLL", "SRL"};

/* ---------------------------------------------------------------------------
 * Instruction length
 * ------------------------------------------------------------------------- */

static int base_length(unsigned char op) {
	switch (op) {
	case 0x01:
	case 0x11:
	case 0x21:
	case 0x31:
		return 3;
	case 0x06:
	case 0x0E:
	case 0x16:
	case 0x1E:
	case 0x26:
	case 0x2E:
	case 0x3E:
		return 2;
	case 0x36:
		return 2;
	case 0x10:
	case 0x18:
	case 0x20:
	case 0x28:
	case 0x30:
	case 0x38:
		return 2;
	case 0x22:
	case 0x2A:
	case 0x32:
	case 0x3A:
		return 3;
	case 0xC2:
	case 0xC3:
	case 0xC4:
	case 0xCA:
	case 0xCC:
	case 0xCD:
	case 0xD2:
	case 0xD4:
	case 0xDA:
	case 0xDC:
		return 3;
	case 0xD3:
	case 0xDB:
		return 2;
	case 0xC6:
	case 0xCE:
	case 0xD6:
	case 0xDE:
	case 0xE6:
	case 0xEE:
	case 0xF6:
	case 0xFE:
		return 2;
	default:
		return 1;
	}
}

/* Does an (HL)-referencing opcode gain a displacement byte under DD/FD? */
static int hl_disp(unsigned char o2) {
	if (o2 == 0x34 || o2 == 0x35 || o2 == 0x36)
		return 1;
	if (o2 >= 0x40 && o2 <= 0x7F) {
		if (o2 == 0x76) /* HALT ignores the prefix */
			return 0;
		if ((o2 & 7) == 6)
			return 1;
		if (o2 >= 0x70 && o2 <= 0x77)
			return 1;
	}
	if (o2 >= 0x80 && o2 <= 0xBF && (o2 & 7) == 6)
		return 1;
	return 0;
}

static int ed_len(unsigned char o2) {
	if (o2 == 0x43 || o2 == 0x4B || o2 == 0x53 || o2 == 0x5B || o2 == 0x63 ||
		o2 == 0x6B || o2 == 0x73 || o2 == 0x7B)
		return 4;
	return 2;
}

unsigned int mon_instr_len(unsigned int addr) {
	unsigned char op, o2;

	op = mon_peek(addr);
	if (op == 0xCB)
		return 2;
	if (op == 0xED)
		return ed_len(mon_peek(addr + 1));
	if (op == 0xDD || op == 0xFD) {
		o2 = mon_peek(addr + 1);
		if (o2 == 0xCB)
			return 4;
		if (hl_disp(o2))
			return 1 + base_length(o2) + 1;
		return 1 + base_length(o2);
	}
	return base_length(op);
}

/* ---------------------------------------------------------------------------
 * Operand printing
 * ------------------------------------------------------------------------- */

static void pr_disp(int p, int d) {
	fputc_cons('(');
	mon_putstr(p == 1 ? "IX" : "IY");
	if (d < 0) {
		fputc_cons('-');
		mon_puthex8((unsigned int)(-d));
	} else {
		fputc_cons('+');
		mon_puthex8((unsigned int)d);
	}
	fputc_cons(')');
}

static void pr_r8(int p, int i, int d) {
	if (p == 0) {
		mon_putstr(r8_name[i]);
		return;
	}
	if (i == 4) {
		mon_putstr(p == 1 ? "IXH" : "IYH");
		return;
	}
	if (i == 5) {
		mon_putstr(p == 1 ? "IXL" : "IYL");
		return;
	}
	if (i == 6) {
		pr_disp(p, d);
		return;
	}
	mon_putstr(r8_name[i]);
}

static void pr_rp(int p, int i) {
	if (p && i == 2) {
		mon_putstr(p == 1 ? "IX" : "IY");
		return;
	}
	mon_putstr(rp_name[i]);
}

/* ---------------------------------------------------------------------------
 * ED-prefixed instructions
 * ------------------------------------------------------------------------- */

static unsigned int disasm_ed(unsigned int b, unsigned int len) {
	unsigned char op = mon_peek(b + 1);
	int z = op & 7;
	int y = (op >> 3) & 7;
	int q = (op >> 4) & 3;

	if (op >= 0x40 && op <= 0x7F) {
		if (z == 0) {
			if (op == 0x70)
				mon_putstr("IN (C)");
			else {
				mon_putstr("IN ");
				mon_putstr(r8_name[y]);
				mon_putstr(",(C)");
			}
			return len;
		}
		if (z == 1) {
			if (op == 0x71)
				mon_putstr("OUT (C),0");
			else {
				mon_putstr("OUT (C),");
				mon_putstr(r8_name[y]);
			}
			return len;
		}
		if (z == 2) {
			mon_putstr((y & 1) ? "ADC HL," : "SBC HL,");
			mon_putstr(rp_name[q]);
			return len;
		}
		if (z == 3) {
			unsigned int nn = (unsigned int)mon_peek(b + 2) |
							  ((unsigned int)mon_peek(b + 3) << 8);
			if (y & 1) {
				mon_putstr("LD ");
				mon_putstr(rp_name[q]);
				mon_putstr(",(0x");
				mon_puthex16(nn);
				fputc_cons(')');
			} else {
				mon_putstr("LD (0x");
				mon_puthex16(nn);
				mon_putstr("),");
				mon_putstr(rp_name[q]);
			}
			return len;
		}
		if (z == 4) {
			mon_putstr("NEG");
			return len;
		}
		if (z == 5) {
			mon_putstr(op == 0x4D ? "RETI" : "RETN");
			return len;
		}
		if (z == 6) {
			mon_putstr("IM ");
			if (op == 0x56 || op == 0x76)
				mon_putstr("1");
			else if (op == 0x5E || op == 0x7E)
				mon_putstr("2");
			else
				mon_putstr("0");
			return len;
		}
		switch (op) {
		case 0x47:
			mon_putstr("LD I,A");
			return len;
		case 0x4F:
			mon_putstr("LD R,A");
			return len;
		case 0x57:
			mon_putstr("LD A,I");
			return len;
		case 0x5F:
			mon_putstr("LD A,R");
			return len;
		case 0x67:
			mon_putstr("RRD");
			return len;
		case 0x6F:
			mon_putstr("RLD");
			return len;
		default:
			break;
		}
	}

	switch (op) {
	case 0xA0:
		mon_putstr("LDI");
		return len;
	case 0xA1:
		mon_putstr("CPI");
		return len;
	case 0xA2:
		mon_putstr("INI");
		return len;
	case 0xA3:
		mon_putstr("OUTI");
		return len;
	case 0xA8:
		mon_putstr("LDD");
		return len;
	case 0xA9:
		mon_putstr("CPD");
		return len;
	case 0xAA:
		mon_putstr("IND");
		return len;
	case 0xAB:
		mon_putstr("OUTD");
		return len;
	case 0xB0:
		mon_putstr("LDIR");
		return len;
	case 0xB1:
		mon_putstr("CPIR");
		return len;
	case 0xB2:
		mon_putstr("INIR");
		return len;
	case 0xB3:
		mon_putstr("OTIR");
		return len;
	case 0xB8:
		mon_putstr("LDDR");
		return len;
	case 0xB9:
		mon_putstr("CPDR");
		return len;
	case 0xBA:
		mon_putstr("INDR");
		return len;
	case 0xBB:
		mon_putstr("OTDR");
		return len;
	default:
		break;
	}

	mon_putstr(".db ED,");
	mon_puthex8(op);
	return len;
}

/* ---------------------------------------------------------------------------
 * Disassemble one instruction, return its length
 * ------------------------------------------------------------------------- */

unsigned int mon_disasm(unsigned int addr) {
	unsigned int len = mon_instr_len(addr);
	unsigned int b = addr;
	int p = 0;
	int d = 0;
	unsigned char op;

	op = mon_peek(b);
	if (op == 0xDD) {
		p = 1;
		b++;
	} else if (op == 0xFD) {
		p = 2;
		b++;
	}
	op = mon_peek(b);

	/* DD/FD CB: rotating/bit ops on (IX+d)/(IY+d) */
	if (p && op == 0xCB) {
		int f = mon_peek(b + 2);
		int dd = (signed char)mon_peek(b + 1);
		int kind = f >> 6;
		if (kind == 0) {
			mon_putstr(rot_name[(f >> 3) & 7]);
			fputc_cons(' ');
			pr_disp(p, dd);
		} else {
			mon_putstr(kind == 1 ? "BIT " : kind == 2 ? "RES " : "SET ");
			mon_putdec((unsigned int)((f >> 3) & 7));
			fputc_cons(',');
			pr_disp(p, dd);
		}
		return len;
	}
	if (op == 0xCB) {
		int f = mon_peek(b + 1);
		int kind = f >> 6;
		int y = (f >> 3) & 7;
		int z = f & 7;
		if (kind == 0) {
			mon_putstr(rot_name[y]);
			fputc_cons(' ');
			pr_r8(0, z, 0);
		} else {
			mon_putstr(kind == 1 ? "BIT " : kind == 2 ? "RES " : "SET ");
			mon_putdec((unsigned int)y);
			fputc_cons(',');
			pr_r8(0, z, 0);
		}
		return len;
	}
	if (op == 0xED)
		return disasm_ed(b, len);

	/* LD r,r' and HALT */
	if (op >= 0x40 && op <= 0x7F) {
		if (op == 0x76) {
			mon_putstr("HALT");
			return len;
		}
		if (p && ((op & 7) == 6 || ((op >> 3) & 7) == 6))
			d = (signed char)mon_peek(b + 1);
		mon_putstr("LD ");
		pr_r8(p, (op >> 3) & 7, d);
		fputc_cons(',');
		pr_r8(p, op & 7, d);
		return len;
	}

	/* ALU A,r */
	if (op >= 0x80 && op <= 0xBF) {
		int z = op & 7;
		mon_putstr(alu_name[(op >> 3) & 7]);
		if (p && z == 6)
			d = (signed char)mon_peek(b + 1);
		pr_r8(p, z, d);
		return len;
	}

	switch (op) {
	case 0x00:
		mon_putstr("NOP");
		break;
	case 0x08:
		mon_putstr("EX AF,AF'");
		break;
	case 0x10:
		mon_putstr("DJNZ ");
		mon_puthex16(
			(unsigned int)((b + 2 + (signed char)mon_peek(b + 1)) & 0xFFFF));
		break;
	case 0x18:
		mon_putstr("JR ");
		mon_puthex16(
			(unsigned int)((b + 2 + (signed char)mon_peek(b + 1)) & 0xFFFF));
		break;
	case 0x20:
	case 0x28:
	case 0x30:
	case 0x38:
		mon_putstr("JR ");
		mon_putstr(cc_name[(op >> 3) & 3]);
		fputc_cons(',');
		mon_puthex16((unsigned int)((b + 2 + (signed char)mon_peek(b + 1)) & 0xFFFF));
		break;

	case 0x01:
	case 0x11:
	case 0x21:
	case 0x31: {
		unsigned int nn = (unsigned int)mon_peek(b + 1) |
						  ((unsigned int)mon_peek(b + 2) << 8);
		mon_putstr("LD ");
		pr_rp(p, (op >> 4) & 3);
		fputc_cons(',');
		mon_puthex16(nn);
		break;
	}
	case 0x09:
	case 0x19:
	case 0x29:
	case 0x39:
		mon_putstr("ADD ");
		pr_rp(p, 2);
		fputc_cons(',');
		pr_rp(p, (op >> 4) & 3);
		break;

	case 0x02:
		mon_putstr("LD (BC),A");
		break;
	case 0x0A:
		mon_putstr("LD A,(BC)");
		break;
	case 0x12:
		mon_putstr("LD (DE),A");
		break;
	case 0x1A:
		mon_putstr("LD A,(DE)");
		break;
	case 0x22:
	case 0x32: {
		unsigned int nn = (unsigned int)mon_peek(b + 1) |
						  ((unsigned int)mon_peek(b + 2) << 8);
		mon_putstr("LD (0x");
		mon_puthex16(nn);
		mon_putstr("),");
		if (op == 0x22)
			pr_rp(p, 2);
		else
			fputc_cons('A');
		break;
	}
	case 0x2A:
	case 0x3A: {
		unsigned int nn = (unsigned int)mon_peek(b + 1) |
						  ((unsigned int)mon_peek(b + 2) << 8);
		if (op == 0x2A) {
			mon_putstr("LD ");
			pr_rp(p, 2);
			mon_putstr(",(0x");
			mon_puthex16(nn);
			fputc_cons(')');
		} else {
			mon_putstr("LD A,(0x");
			mon_puthex16(nn);
			fputc_cons(')');
		}
		break;
	}

	case 0x03:
	case 0x13:
	case 0x23:
	case 0x33:
		mon_putstr("INC ");
		pr_rp(p, (op >> 4) & 3);
		break;
	case 0x0B:
	case 0x1B:
	case 0x2B:
	case 0x3B:
		mon_putstr("DEC ");
		pr_rp(p, (op >> 4) & 3);
		break;

	case 0x04:
	case 0x0C:
	case 0x14:
	case 0x1C:
	case 0x24:
	case 0x2C:
	case 0x34:
	case 0x3C:
		if (p && (op & 7) == 6)
			d = (signed char)mon_peek(b + 1);
		mon_putstr("INC ");
		pr_r8(p, (op >> 3) & 7, d);
		break;
	case 0x05:
	case 0x0D:
	case 0x15:
	case 0x1D:
	case 0x25:
	case 0x2D:
	case 0x35:
	case 0x3D:
		if (p && (op & 7) == 6)
			d = (signed char)mon_peek(b + 1);
		mon_putstr("DEC ");
		pr_r8(p, (op >> 3) & 7, d);
		break;
	case 0x06:
	case 0x0E:
	case 0x16:
	case 0x1E:
	case 0x26:
	case 0x2E:
	case 0x3E:
		mon_putstr("LD ");
		pr_r8(p, (op >> 3) & 7, 0);
		fputc_cons(',');
		mon_puthex8(mon_peek(b + 1));
		break;
	case 0x36:
		if (p)
			d = (signed char)mon_peek(b + 1);
		mon_putstr("LD ");
		pr_r8(p, 6, d);
		fputc_cons(',');
		mon_puthex8(mon_peek(b + (p ? 2 : 1)));
		break;

	case 0x07:
		mon_putstr("RLCA");
		break;
	case 0x0F:
		mon_putstr("RRCA");
		break;
	case 0x17:
		mon_putstr("RLA");
		break;
	case 0x1F:
		mon_putstr("RRA");
		break;
	case 0x27:
		mon_putstr("DAA");
		break;
	case 0x2F:
		mon_putstr("CPL");
		break;
	case 0x37:
		mon_putstr("SCF");
		break;
	case 0x3F:
		mon_putstr("CCF");
		break;

	case 0xC0:
	case 0xC8:
	case 0xD0:
	case 0xD8:
	case 0xE0:
	case 0xE8:
	case 0xF0:
	case 0xF8:
		mon_putstr("RET ");
		mon_putstr(cc_name[(op >> 3) & 7]);
		break;
	case 0xC9:
		mon_putstr("RET");
		break;
	case 0xC1:
	case 0xC5:
	case 0xD1:
	case 0xD5:
	case 0xE1:
	case 0xE5:
	case 0xF1:
	case 0xF5:
		mon_putstr((op & 0x04) ? "PUSH " : "POP ");
		if (p && ((op >> 4) & 3) == 2)
			mon_putstr(p == 1 ? "IX" : "IY");
		else
			mon_putstr(rp2_name[(op >> 4) & 3]);
		break;

	case 0xC2:
	case 0xCA:
	case 0xD2:
	case 0xDA:
	case 0xE2:
	case 0xEA:
	case 0xF2:
	case 0xFA: {
		unsigned int nn = (unsigned int)mon_peek(b + 1) |
						  ((unsigned int)mon_peek(b + 2) << 8);
		mon_putstr("JP ");
		mon_putstr(cc_name[(op >> 3) & 7]);
		fputc_cons(',');
		mon_puthex16(nn);
		break;
	}
	case 0xC3: {
		unsigned int nn = (unsigned int)mon_peek(b + 1) |
						  ((unsigned int)mon_peek(b + 2) << 8);
		mon_putstr("JP ");
		mon_puthex16(nn);
		break;
	}
	case 0xC4:
	case 0xCC:
	case 0xD4:
	case 0xDC:
	case 0xE4:
	case 0xEC:
	case 0xF4:
	case 0xFC: {
		unsigned int nn = (unsigned int)mon_peek(b + 1) |
						  ((unsigned int)mon_peek(b + 2) << 8);
		mon_putstr("CALL ");
		mon_putstr(cc_name[(op >> 3) & 7]);
		fputc_cons(',');
		mon_puthex16(nn);
		break;
	}
	case 0xCD: {
		unsigned int nn = (unsigned int)mon_peek(b + 1) |
						  ((unsigned int)mon_peek(b + 2) << 8);
		mon_putstr("CALL ");
		mon_puthex16(nn);
		break;
	}

	case 0xC6:
	case 0xCE:
	case 0xD6:
	case 0xDE:
	case 0xE6:
	case 0xEE:
	case 0xF6:
	case 0xFE:
		mon_putstr(alu_name[(op >> 3) & 7]);
		mon_puthex8(mon_peek(b + 1));
		break;

	case 0xC7:
	case 0xCF:
	case 0xD7:
	case 0xDF:
	case 0xE7:
	case 0xEF:
	case 0xF7:
	case 0xFF:
		mon_putstr("RST ");
		mon_puthex8(op & 0x38);
		break;

	case 0xD3:
		mon_putstr("OUT (");
		mon_puthex8(mon_peek(b + 1));
		mon_putstr("),A");
		break;
	case 0xDB:
		mon_putstr("IN A,(");
		mon_puthex8(mon_peek(b + 1));
		fputc_cons(')');
		break;
	case 0xD9:
		mon_putstr("EXX");
		break;
	case 0xE3:
		mon_putstr("EX (SP),");
		pr_rp(p, 2);
		break;
	case 0xE9:
		mon_putstr("JP (");
		pr_rp(p, 2);
		fputc_cons(')');
		break;
	case 0xEB:
		mon_putstr("EX DE,HL");
		break;
	case 0xF3:
		mon_putstr("DI");
		break;
	case 0xFB:
		mon_putstr("EI");
		break;
	case 0xF9:
		mon_putstr("LD SP,");
		pr_rp(p, 2);
		break;

	default:
		mon_putstr(".db ");
		mon_puthex8(op);
		break;
	}
	return len;
}

/* ---------------------------------------------------------------------------
 * "U [addr]" command
 * ------------------------------------------------------------------------- */

static unsigned int disasm_addr = 0;

static int local_parse_word(char **pp, unsigned int *out) {
	char *p = *pp;
	unsigned int v = 0;
	int n = 0;
	int d;

	while (*p == ' ' || *p == '\t')
		p++;
	for (;;) {
		d = -1;
		if (*p >= '0' && *p <= '9')
			d = *p - '0';
		else if (*p >= 'a' && *p <= 'f')
			d = *p - 'a' + 10;
		else if (*p >= 'A' && *p <= 'F')
			d = *p - 'A' + 10;
		if (d < 0)
			break;
		v = ((v << 4) | (unsigned int)d) & 0xFFFF;
		p++;
		n++;
		if (n >= 4)
			break;
	}
	if (n == 0)
		return 0;
	*pp = p;
	*out = v;
	return 1;
}

void cmd_disasm(char *p) {
	unsigned int a;
	int i;

	if (local_parse_word(&p, &a))
		disasm_addr = a & 0xFFFF;

	for (i = 0; i < 8; i++) {
		unsigned int len;
		mon_puthex16(disasm_addr);
		mon_putstr(": ");
		len = mon_disasm(disasm_addr);
		fputc_cons('\n');
		disasm_addr = (disasm_addr + len) & 0xFFFF;
	}
}
