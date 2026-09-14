// hardfault.c — Cortex-M33 hard-fault handler with UART diagnostics
// z80neo — TurBoss 2026
//
// Overrides the pico-sdk's weak isr_hardfault (which just loops on a bkpt).
// On a hard fault, dumps the fault-status registers and the exception stack
// frame to UART0 so the crash cause is visible on the serial console.

#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"

// System Control Block fault-status registers (PPB / System Control Space)
#define SCB_CFSR  (*(volatile uint32_t *)0xE000ED28u)  // Configurable Fault Status
#define SCB_HFSR  (*(volatile uint32_t *)0xE000ED2Cu)  // Hard Fault Status
#define SCB_MMFAR (*(volatile uint32_t *)0xE000ED34u)  // MemManage Fault Address
#define SCB_BFAR  (*(volatile uint32_t *)0xE000ED38u)  // Bus Fault Address

// Blocking byte output directly to UART0 (FIFO is drained by hardware).
static void hf_putc(char c) {
    while (uart_get_hw(uart0)->fr & UART_UARTFR_TXFF_BITS) tight_loop_contents();
    uart_get_hw(uart0)->dr = c;
}

static void hf_puts(const char *s) {
    while (*s) hf_putc(*s++);
}

static void hf_hex(uint32_t v) {
    hf_puts("0x");
    for (int i = 28; i >= 0; i -= 4) {
        uint8_t n = (uint8_t)((v >> i) & 0xF);
        hf_putc(n < 10 ? (char)('0' + n) : (char)('A' + n - 10));
    }
}

void isr_hardfault(void) {
    // Exception frame is at MSP (we are in Handler mode).
    uint32_t msp;
    __asm__ volatile("mrs %0, msp" : "=r"(msp));
    const uint32_t *f = (const uint32_t *)msp;

    uint32_t r0 = f[0], r1 = f[1], r2 = f[2], r3 = f[3];
    uint32_t r12 = f[4], lr = f[5], pc = f[6], xpsr = f[7];

    uint32_t cfsr = SCB_CFSR;
    uint32_t hfsr = SCB_HFSR;
    uint32_t bfar = SCB_BFAR;
    uint32_t mmfar = SCB_MMFAR;

    hf_puts("\r\n\r\n=== HARDFAULT ===\r\n");
    hf_puts("CFSR=");  hf_hex(cfsr);  hf_puts("\r\n");
    hf_puts("HFSR=");  hf_hex(hfsr);  hf_puts("\r\n");
    hf_puts("BFAR=");  hf_hex(bfar);  hf_puts("\r\n");
    hf_puts("MMFAR="); hf_hex(mmfar); hf_puts("\r\n");
    hf_puts("PC=");    hf_hex(pc);    hf_puts("\r\n");
    hf_puts("LR=");    hf_hex(lr);    hf_puts("\r\n");
    hf_puts("R0=");  hf_hex(r0);
    hf_puts(" R1="); hf_hex(r1);
    hf_puts(" R2="); hf_hex(r2);
    hf_puts(" R3="); hf_hex(r3);
    hf_puts(" R12="); hf_hex(r12);
    hf_puts(" xPSR="); hf_hex(xpsr);
    hf_puts("\r\n");

    // Decode CFSR bits (usage / bus / memmanage faults)
    if (cfsr & (1u << 25)) hf_puts("  UFSR DIVBYZERO\r\n");
    if (cfsr & (1u << 24)) hf_puts("  UFSR UNALIGNED\r\n");
    if (cfsr & (1u << 19)) hf_puts("  UFSR NOCP (VFP/coprocessor)\r\n");
    if (cfsr & (1u << 18)) hf_puts("  UFSR INVPC\r\n");
    if (cfsr & (1u << 17)) hf_puts("  UFSR INVSTATE\r\n");
    if (cfsr & (1u << 16)) hf_puts("  UFSR UNDEFINSTR\r\n");
    if (cfsr & (1u << 15)) hf_puts("  BFSR BFARVALID\r\n");
    if (cfsr & (1u << 12)) hf_puts("  BFSR PRECISERR\r\n");
    if (cfsr & (1u << 11)) hf_puts("  BFSR IMPRECISERR\r\n");
    if (cfsr & (1u << 10)) hf_puts("  BFSR UNSTKERR\r\n");
    if (cfsr & (1u << 9))  hf_puts("  BFSR STKERR\r\n");
    if (cfsr & (1u << 8))  hf_puts("  BFSR IBUSERR\r\n");
    if (cfsr & (1u << 7))  hf_puts("  MMFSR MMARVALID\r\n");
    if (cfsr & (1u << 4))  hf_puts("  MMFSR MSTKERR\r\n");
    if (cfsr & (1u << 3))  hf_puts("  MMFSR MUNSTKERR\r\n");
    if (cfsr & (1u << 1))  hf_puts("  MMFSR DACCVIOL\r\n");
    if (cfsr & (1u << 0))  hf_puts("  MMFSR IACCVIOL\r\n");

    hf_puts("=== HALTED ===\r\n");

    while (1) tight_loop_contents();
}
