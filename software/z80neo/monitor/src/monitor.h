/*
 * monitor.h — z80neo machine monitor / debugger
 *
 * Shared definitions between the C code and mon_asm.asm.
 */

#ifndef MONITOR_H
#define MONITOR_H

#include <stdint.h>

/* Saved CPU registers, written by asm_im1_handler and read/written by C.
 * Kept as an array of 16-bit words: mon_asm.asm addresses elements as
 * _mon_regs + 2*index. */
#define R_AF 0
#define R_BC 1
#define R_DE 2
#define R_HL 3
#define R_AF2 4
#define R_BC2 5
#define R_DE2 6
#define R_HL2 7
#define R_IX 8
#define R_IY 9
#define R_SP 10
#define R_PC 11
#define R_IR 12
#define R_COUNT 13

extern unsigned int mon_regs[R_COUNT];

/* Low-level routines in mon_asm.asm */
void mon_run(void);
unsigned int mon_mem_top(void);

/* The C monitor body (never returns) */
void monitor_body(void);

/* Memory / port access (monitor.c) */
unsigned char mon_peek(unsigned int addr);
void mon_poke(unsigned int addr, unsigned char value);
unsigned char mon_in(unsigned int port);
void mon_out(unsigned int port, unsigned char value);

/* Minimal formatting (monitor.c) — avoids pulling the big classic printf. */
void mon_putstr(const char *s);
void mon_puthex8(unsigned int v);
void mon_puthex16(unsigned int v);
void mon_putdec(unsigned int v);

/* Instruction length / disassembly (disasm.c) */
unsigned int mon_instr_len(unsigned int addr);
unsigned int mon_disasm(unsigned int addr);
void cmd_disasm(char *p);

#endif /* MONITOR_H */
