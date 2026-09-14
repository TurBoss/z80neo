// z80bus_pio.h — PIO-accelerated Z80 bus handler
// Replaces ARM-side bus_poll() with PIO-assisted cycle capture.
// Targets Raspberry Pi Pico 2 (RP2350 / BB48).

#ifndef Z80BUS_PIO_H
#define Z80BUS_PIO_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------------------
// Bus cycle capture word (pushed by PIO to RX FIFO via autopush)
// ---------------------------------------------------------------------------
// The PIO samples the input pins (IN_BASE = GPIO12) at the moment the cycle
// starts and autopushes a 32-bit snapshot.  The ARM only uses the word as a
// "cycle detected" notification and re-reads the live pins for the actual
// address/data/control signals, so the bit layout is informational only:
//   bits  0-7  = GPIO12-19 (D0-D7 / A0-7 / A8-15 via the SEL mux)
//   bits  8-11 = GPIO20-23 (MREQ, RD, IORQ, WR — active LOW, 0 = asserted)
//   bits 12-31 = unused (GPIO24+)

// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------

// Initialize the PIO bus handler.
// Must be called after GPIO pins are configured and before releasing Z80 reset.
// z80_khz: Z80 clock in kHz.  Accepted for API stability; the handler is
// WAIT-stalled so it does not currently need a clock-dependent PIO divider.
void z80bus_pio_init(unsigned int z80_khz);

// Process captured bus cycles (call from main loop).
// Returns number of cycles processed.
uint32_t z80bus_pio_poll(void);

// Print statistics to UART.
void z80bus_pio_stats(void);

// Check if PIO bus handler is active.
bool z80bus_pio_is_active(void);

// Suspend PIO handler (e.g., during flash operations).
void z80bus_pio_suspend(void);

// Resume PIO handler.
void z80bus_pio_resume(void);

// Check if both PIO FIFOs are empty (Z80 is idle — safe to do slow I/O)
bool z80bus_pio_is_idle(void);

// Timing diagnostics: clear / dump the per-cycle trace ring.
void z80bus_pio_diag_reset(void);
void z80bus_pio_dump(void);

// Set from gdb to request a dump/reset from the main loop.
extern volatile bool z80bus_diag_dump;
extern volatile bool z80bus_diag_reset;

// ---------------------------------------------------------------------------
// Statistics (exported for display)
// ---------------------------------------------------------------------------

extern volatile uint32_t pio_cycles_total;
extern volatile uint32_t pio_cycles_read;
extern volatile uint32_t pio_cycles_write;
extern volatile uint32_t pio_cycles_io;
extern volatile uint32_t pio_cycles_refresh;

// Bus diagnostics (shown on the display, useful when tuning the bus timing).
extern volatile uint32_t diag_mem_timeouts;
extern volatile uint32_t diag_io_timeouts;
extern volatile uint32_t diag_max_service_us;

#ifdef __cplusplus
}
#endif

#endif // Z80BUS_PIO_H
