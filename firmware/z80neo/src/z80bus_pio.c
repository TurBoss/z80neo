// z80bus_pio.c — PIO-accelerated Z80 bus handler (dual-SM capture)
// SM0 (mem_sm=0): JMP_PIN = GPIO20 (MREQ), SET pin = GPIO7  -> Z80 WAIT (pin 24)
// SM1 (io_sm=1):  JMP_PIN = GPIO22 (IORQ), SET pin = GPIO4 -> Z80 WAIT (pin 24)
//
// The PIO asserts WAIT the moment a bus cycle starts and holds the Z80 stalled
// until the ARM has serviced the cycle, then releases WAIT and waits for the
// cycle to really end before re-arming.  Because the Z80 is stalled for the
// whole service time, this handler is correct at any Z80 clock frequency.

#include <stdio.h>
#include "z80bus_pio.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"
#include "z80bus_pio.pio.h"
#include "memory.h"

static PIO bus_pio = NULL;
static unsigned int mem_sm = 0, io_sm = 1;
static bool pio_active = false;

volatile uint32_t pio_cycles_total = 0;
volatile uint32_t pio_cycles_read  = 0;
volatile uint32_t pio_cycles_write = 0;
volatile uint32_t pio_cycles_io    = 0;
volatile uint32_t pio_cycles_refresh = 0;

volatile uint16_t diag_addr = 0;
volatile uint8_t  diag_drive = 0;
volatile uint8_t  diag_gpio = 0;
volatile uint8_t  diag_io_port = 0;
volatile uint8_t  diag_io_data = 0;
volatile uint8_t  diag_io_hi = 0;

#define DIAG_FIRST_N 64
volatile uint16_t diag_first[DIAG_FIRST_N];
volatile uint8_t  diag_first_data[DIAG_FIRST_N];
volatile uint8_t  diag_first_al[DIAG_FIRST_N];
volatile uint8_t  diag_first_ah[DIAG_FIRST_N];
volatile uint32_t diag_first_idx = 0;

// Ring buffer of the most recent memory reads (to catch a late derail)
volatile uint16_t diag_last_addr[DIAG_FIRST_N];
volatile uint8_t  diag_last_data[DIAG_FIRST_N];
volatile uint8_t  diag_last_al[DIAG_FIRST_N];
volatile uint8_t  diag_last_ah[DIAG_FIRST_N];
volatile uint32_t diag_last_idx = 0;

// I/O read ring: port/data pairs (to catch port mis-decodes).
volatile uint8_t  diag_ior_port[64];
volatile uint8_t  diag_ior_data[64];
volatile uint32_t diag_ior_idx = 0;

// ---- PIO timing diagnostics ----------------------------------------------
// Per-cycle trace.  `kind`: 0=mem rd, 1=mem wr, 2=io rd, 3=io wr,
// 4=refresh skip, 5=io glitch/M1 skip.  `pc0`/`pc1` are the mem/io PIO SM
// program counters sampled while the cycle is serviced, so a stuck SM shows up
// as a constant PC.  `ctl` is the live control-line snapshot (bit0 MREQ,
// bit1 RD, bit2 IORQ, bit3 WR; a set bit means the line was HIGH/inactive).
typedef struct {
    uint32_t t_us;
    uint16_t addr;
    uint8_t  data;
    uint8_t  kind;
    uint8_t  pc0;
    uint8_t  pc1;
    uint8_t  ctl;
} diag_cyc_t;
#define DIAG_CYC_N 256
volatile diag_cyc_t diag_cyc[DIAG_CYC_N];
volatile uint32_t diag_cyc_idx = 0;

volatile uint32_t diag_mem_timeouts = 0;   // MREQ never went HIGH in handle_mem
volatile uint32_t diag_io_timeouts  = 0;   // RD/WR never asserted in handle_iorq
volatile uint32_t diag_m1_skips     = 0;   // io SM fired on an M1 fetch
volatile uint32_t diag_max_service_us = 0;

// Set these from gdb to trigger a trace dump / reset from the main loop.
volatile bool z80bus_diag_dump  = false;
volatile bool z80bus_diag_reset = false;

// Bus settle times (µs).  These are a property of the analog bus (mux +
// transceiver OE/propagation), not of CPU_SPEED, and they are hidden by WAIT
// (the Z80 is stalled for the whole service).  They dominate Z80 throughput:
// sum per memory access = BUS_ADDR_LO + BUS_ADDR_HI + BUS_DATA.  The Z80 bus
// is static while stalled and the 74LVC245-class transceivers switch in
// nanoseconds, so these are far smaller than the original 20/10/40/40, which
// were tuned against an older board.  1/1/2/2 is validated on the current
// board (fast, clean CP/M); raise DATA first if data-bit errors reappear
// (e.g. an '8' read back as '9'), then the address/IO values.
#define BUS_DATA_SETTLE_US    2
#define BUS_ADDR_LO_SETTLE_US 1
#define BUS_ADDR_HI_SETTLE_US 1
#define BUS_IO_PORT_SETTLE_US 2

// Short mux-switch settle (~300 ns) for reading the address through SEL1/SEL2.
static inline void bus_settle(void) {
    __asm__ volatile(
        "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
        "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
        "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
        "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
        "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop" ::: "memory");
}

// Sample the 8-bit bus and require two consecutive identical reads so a mux or
// level shifter that is still slewing cannot be latched.  The Z80 is held in
// WAIT for the whole service, so the bus is static and this always settles.
static inline uint8_t bus_read_stable(void) {
    uint8_t v = ((gpio_get_all() & bus_mask) >> BUS_GPIO_START) & 0xFF;
    for (int i = 0; i < 8; i++) {
        uint8_t n = ((gpio_get_all() & bus_mask) >> BUS_GPIO_START) & 0xFF;
        if (n == v) return v;
        v = n;
    }
    return v;
}

// Record one serviced (or skipped) cycle into the trace ring.
static inline void diag_record(uint8_t kind, uint16_t addr, uint8_t data) {
    uint32_t i = diag_cyc_idx & (DIAG_CYC_N - 1);
    diag_cyc[i].t_us = time_us_32();
    diag_cyc[i].addr = addr;
    diag_cyc[i].data = data;
    diag_cyc[i].kind = kind;
    diag_cyc[i].pc0  = pio_sm_get_pc(bus_pio, mem_sm);
    diag_cyc[i].pc1  = pio_sm_get_pc(bus_pio, io_sm);
    diag_cyc[i].ctl  = (gpio_get(MREQ_INPUT) ? 1 : 0) |
                       (gpio_get(RD_INPUT)   ? 2 : 0) |
                       (gpio_get(IORQ_INPUT) ? 4 : 0) |
                       (gpio_get(WR_INPUT)   ? 8 : 0);
    diag_cyc_idx++;
}

static void handle_iorq(void) {
    // An M1 opcode fetch asserts IORQ *and* MREQ.  The io SM triggers on IORQ,
    // so if MREQ is also low this is an M1 — the mem SM services it.  Just
    // release our WAIT and let the mem SM handle the fetch.
    if (!gpio_get(MREQ_INPUT)) {
        pio_sm_put(bus_pio, io_sm, 0);
        pio_cycles_io++;
        diag_m1_skips++;
        diag_record(5, 0, 0);
        return;
    }
    // PIO holds WAIT (GPIO4=0) — Z80 is stopped
    gpio_put(SEL3_OUT, 1); gpio_put(SEL1_OUT, 1); gpio_put(SEL2_OUT, 1);
    gpio_set_dir_masked(bus_mask, 0);
    // Wait for WR or RD to assert (sampled during T2/TW, Z80 is stalled)
    int to = 20000;
    while (gpio_get(WR_INPUT) && gpio_get(RD_INPUT) && --to) tight_loop_contents();
    if (!to) {  // INTACK / glitch — nothing to service, just release
        pio_sm_put(bus_pio, io_sm, 0);
        pio_cycles_io++;
        diag_io_timeouts++;
        diag_record(5, 0, 0);
        return;
    }
    gpio_put(SEL2_OUT, 1); gpio_put(SEL1_OUT, 0);
    busy_wait_us_32(BUS_IO_PORT_SETTLE_US);
    uint8_t port = bus_read_stable();
    gpio_put(SEL1_OUT, 1); gpio_put(SEL2_OUT, 1);

    if (!gpio_get(WR_INPUT)) {
        gpio_put(SEL3_OUT, 0); gpio_put(DIR3_OUT, 0);

        busy_wait_us_32(BUS_DATA_SETTLE_US);

        io_w_op = bus_read_stable();
        gpio_put(SEL3_OUT, 1);
        // Also sample A8-A15 (the Z80 places OUT data on the high addr byte too)
        gpio_put(SEL2_OUT, 0);
        busy_wait_us_32(BUS_DATA_SETTLE_US);
        diag_io_hi = bus_read_stable();
        gpio_put(SEL2_OUT, 1);
        diag_io_port = port; diag_io_data = io_w_op;
        io_write_port(port, io_w_op); io_op++;
        diag_record(3, port, io_w_op);
        pio_sm_put(bus_pio, io_sm, 0);  // release PIO WAIT
    } else if (!gpio_get(RD_INPUT)) {
        io_r_op = io_read_port(port); io_op++;
        diag_io_port = port; diag_io_data = io_r_op;
        diag_record(2, port, io_r_op);
        if (port == 0x80 || port == 0x81 || port == 0x83) {
            diag_ior_port[diag_ior_idx & 63] = port;
            diag_ior_data[diag_ior_idx & 63] = io_r_op;
            diag_ior_idx++;
        }
        gpio_put(SEL3_OUT, 0); gpio_put(DIR3_OUT, 1);
        gpio_put_masked(bus_mask, (uint32_t)io_r_op << BUS_GPIO_START);
        set_bus_dir(1);

        // Let the data propagate to the Z80 pins before releasing WAIT.
        busy_wait_us_32(BUS_DATA_SETTLE_US);

        pio_sm_put(bus_pio, io_sm, 0);  // release PIO WAIT
        // Hold the data until the Z80 has latched it (T3), then release the bus
        int t2 = 2000;
        while (!gpio_get(IORQ_INPUT) && --t2) tight_loop_contents();
        gpio_set_dir_masked(bus_mask, 0); gpio_put(SEL3_OUT, 1);
    } else {
        pio_sm_put(bus_pio, io_sm, 0);
    }
    pio_cycles_io++;
}

static void handle_mem(void) {
    uint32_t t0 = time_us_32();
    // PIO holds WAIT (GPIO7=0) — Z80 is stopped.
    //
    // A Z80 refresh cycle asserts MREQ (RFSH is not wired on this board) but
    // neither RD nor WR, so the mem SM fires on it.  Wait here for RD/WR to
    // assert (real cycle, T2) or for MREQ to go HIGH (refresh ended).  Doing
    // this BEFORE the slow address read is essential: otherwise the read runs
    // past the end of the refresh and the following opcode fetch gets
    // mis-serviced, corrupting the Z80's instruction stream.
    int to = 50000;
    while (gpio_get(RD_INPUT) && gpio_get(WR_INPUT) && !gpio_get(MREQ_INPUT) && --to)
        tight_loop_contents();
    if (gpio_get(RD_INPUT) && gpio_get(WR_INPUT)) {
        // Refresh / spurious: release WAIT and re-arm without servicing.
        pio_sm_put(bus_pio, mem_sm, 0);
        pio_cycles_refresh++;
        diag_record(4, 0, 0);
        return;
    }

    // Real read or write: RD/WR asserted and the Z80 is stalled in T2/TW.
    // Give the Z80 time to finish driving the address before sampling the mux.
    busy_wait_us_32(2);
    gpio_put(SEL3_OUT, 1); gpio_put(SEL2_OUT, 1);
    gpio_put(SEL1_OUT, 0);
    // Settle the mux + level shifter before sampling the address low byte.
    busy_wait_us_32(BUS_ADDR_LO_SETTLE_US);
    uint8_t al = bus_read_stable();
    uint32_t m = (1u << SEL1_OUT) | (1u << SEL2_OUT);
    gpio_put_masked(m, (1u << SEL1_OUT));
    // Settle the mux + level shifter before sampling the address high byte.
    busy_wait_us_32(BUS_ADDR_HI_SETTLE_US);
    uint8_t ah = bus_read_stable();
    gpio_put(SEL2_OUT, 1);
    uint16_t addr = al | ((uint16_t)ah << 8);

    if (!gpio_get(RD_INPUT)) {
        uint8_t r = mmu_read(addr);
        gpio_put(SEL3_OUT, 0); gpio_put(DIR3_OUT, 1);
        bus_settle();

        gpio_put_masked(bus_mask, (uint32_t)r << BUS_GPIO_START);
        gpio_set_dir_masked(bus_mask, bus_mask);
        // Let the data propagate to the Z80 pins before releasing WAIT.
        busy_wait_us_32(BUS_DATA_SETTLE_US);
        // diag: what we drove vs what is on the pins
        diag_addr = addr; diag_drive = r;
        diag_gpio = (gpio_get_all() & bus_mask) >> BUS_GPIO_START;
        if (diag_first_idx < DIAG_FIRST_N) {
            diag_first[diag_first_idx] = addr;
            diag_first_data[diag_first_idx] = r;
            diag_first_al[diag_first_idx] = al;
            diag_first_ah[diag_first_idx] = ah;
            diag_first_idx++;
        }
        diag_last_addr[diag_last_idx & (DIAG_FIRST_N-1)] = addr;
        diag_last_data[diag_last_idx & (DIAG_FIRST_N-1)] = r;
        diag_last_al[diag_last_idx & (DIAG_FIRST_N-1)] = al;
        diag_last_ah[diag_last_idx & (DIAG_FIRST_N-1)] = ah;
        diag_last_idx++;
        pio_sm_put(bus_pio, mem_sm, 0);  // release PIO WAIT
        // Keep the data on the bus until the Z80 has latched it at the T3
        // rising edge (MREQ goes HIGH at cycle end) — releasing earlier at
        // 1 MHz would let the Z80 sample garbage.  Bounded so a stuck MREQ
        // can't hang the main loop (20 ms max).
        // Poll tightly: at 1 MHz the MREQ-high window is only ~1 µs, so a
        // 1 µs-granularity poll misses it and the access stalls until the
        // timeout.  Draining the io SM here prevents an IORQ deadlock.
        int mto = 200000;
        while (!gpio_get(MREQ_INPUT) && --mto) {
            while (!pio_sm_is_rx_fifo_empty(bus_pio, io_sm)) {
                pio_sm_get(bus_pio, io_sm); handle_iorq();
            }
            tight_loop_contents();
        }
        if (!mto) diag_mem_timeouts++;

        // MREQ going HIGH coincides with the Z80's T3 data latch; a short
        // settle past that edge is enough.  A longer fixed hold would overlap
        // the next cycle's data phase above ~1 MHz and cause bus contention.
        bus_settle();

        gpio_set_dir_masked(bus_mask, 0);

        gpio_put(SEL3_OUT, 1); gpio_put(DIR3_OUT, 0);
        pio_cycles_read++; m_adr = addr; mem_r_op = r; d_adr = addr; dr_op++;
        diag_record(0, addr, r);
    } else if (!gpio_get(WR_INPUT)) {
        gpio_put(SEL3_OUT, 0); gpio_put(DIR3_OUT, 0);

        busy_wait_us_32(BUS_DATA_SETTLE_US);

        uint8_t d = bus_read_stable();
        gpio_put(SEL3_OUT, 1);
        mmu_write(addr, d);
        pio_sm_put(bus_pio, mem_sm, 0);  // release PIO WAIT

        int mto = 200000;
        while (!gpio_get(MREQ_INPUT) && --mto) {
            while (!pio_sm_is_rx_fifo_empty(bus_pio, io_sm)) {
                pio_sm_get(bus_pio, io_sm); handle_iorq();
            }
            tight_loop_contents();
        }
        if (!mto) diag_mem_timeouts++;

        busy_wait_us_32(BUS_DATA_SETTLE_US);
        // Debug: pause 1 ms after the read cycle completes (Z80 has latched
        // the data and the bus is released) — drastically slows Z80 execution

        //  busy_wait_us_32(100);

        gpio_set_dir_masked(bus_mask, 0);
        gpio_put(SEL3_OUT, 1); gpio_put(DIR3_OUT, 0);
        pio_cycles_write++; m_adr = addr; mem_w_op = d; d_adr = addr; dw_op++;
        diag_record(1, addr, d);
    } else {
        pio_sm_put(bus_pio, mem_sm, 0);  // shouldn't happen, but don't hang
    }
    uint32_t dt = time_us_32() - t0;
    if (dt > diag_max_service_us) diag_max_service_us = dt;
    pio_cycles_total++;
}

uint32_t z80bus_pio_poll(void) {
    if (!pio_active) return 0; uint32_t d = 0;
    // Process I/O cycles first.  During an M1 (opcode fetch) both SMs fire;
    // servicing the io SM first lets handle_iorq see MREQ still LOW and skip
    // the fetch, releasing its WAIT (GPIO4) before handle_mem runs.  If memory
    // were serviced first, handle_mem would wait for MREQ to go HIGH while the
    // io SM still held WAIT, stalling 20 ms and then releasing the data bus
    // before the Z80 could latch the opcode.
    while (!pio_sm_is_rx_fifo_empty(bus_pio, io_sm)) {
        pio_sm_get(bus_pio, io_sm); handle_iorq(); d++;
    }
    // Then memory cycles.
    while (!pio_sm_is_rx_fifo_empty(bus_pio, mem_sm)) {
        pio_sm_get(bus_pio, mem_sm); handle_mem(); d++;
    }
    return d;
}

// Clear the timing trace so the next run starts from an empty ring.
__attribute__((used)) void z80bus_pio_diag_reset(void) {
    diag_cyc_idx = 0;
    diag_mem_timeouts = 0;
    diag_io_timeouts = 0;
    diag_m1_skips = 0;
    diag_max_service_us = 0;
}

// Dump the timing trace to UART.  Each line: timestamp(us) kind addr data
// ctl pc0 pc1.  kind: 0=mem rd 1=mem wr 2=io rd 3=io wr 4=refresh 5=skip.
__attribute__((used)) void z80bus_pio_dump(void) {
    char b[128];
    uint32_t n = diag_cyc_idx < DIAG_CYC_N ? diag_cyc_idx : DIAG_CYC_N;
    uint32_t start = diag_cyc_idx - n;
    sprintf(b, "PIO diag: %lu cycles, mem_to=%lu io_to=%lu m1=%lu max=%luus\r\n",
            (unsigned long)diag_cyc_idx, (unsigned long)diag_mem_timeouts,
            (unsigned long)diag_io_timeouts, (unsigned long)diag_m1_skips,
            (unsigned long)diag_max_service_us);
    uart_puts(UART_ID, b);
    for (uint32_t k = 0; k < n; k++) {
        uint32_t i = (start + k) & (DIAG_CYC_N - 1);
        sprintf(b, "%8lu k%u a=%04x d=%02x ctl=%x pc0=%02x pc1=%02x\r\n",
                (unsigned long)diag_cyc[i].t_us, diag_cyc[i].kind,
                diag_cyc[i].addr, diag_cyc[i].data, diag_cyc[i].ctl,
                diag_cyc[i].pc0, diag_cyc[i].pc1);
        uart_puts(UART_ID, b);
    }
}

void z80bus_pio_init(unsigned int z80_khz) {
    if (pio_active) return;
    bus_pio = pio0; mem_sm = 0; io_sm = 1;
    (void)z80_khz;

    // WAIT is open-drain: enable the pad pull-ups and let the PIO drive the
    // pins LOW (assert) or high-Z (release).  Push-pull would contend because
    // both SMs share the same Z80 WAIT net.
    gpio_init(4); gpio_set_dir(4, GPIO_IN); gpio_pull_up(4);
    gpio_set_function(4, GPIO_FUNC_PIO0);
    gpio_init(7); gpio_set_dir(7, GPIO_IN); gpio_pull_up(7);
    gpio_set_function(7, GPIO_FUNC_PIO0);

    // Load the two shared PIO programs (one per SM)
    uint off_mem = pio_add_program(bus_pio, &z80bus_pio_mem_program);
    uint off_io  = pio_add_program(bus_pio, &z80bus_pio_io_program);

    // MEM SM: JMP_PIN=20 (MREQ), SET pins=GPIO7
    pio_sm_config cm = z80bus_pio_mem_program_get_default_config(off_mem);
    sm_config_set_in_pins(&cm, 12);
    sm_config_set_set_pins(&cm, 7, 1);
    sm_config_set_jmp_pin(&cm, 20);
    sm_config_set_in_shift(&cm, false, true, 32);
    sm_config_set_out_shift(&cm, true, false, 32);
    sm_config_set_clkdiv(&cm, 1.0f);
    pio_sm_set_consecutive_pindirs(bus_pio, mem_sm, 7, 1, false);
    pio_sm_set_consecutive_pindirs(bus_pio, mem_sm, 12, 16, false);
    pio_sm_init(bus_pio, mem_sm, off_mem, &cm);
    pio_sm_set_enabled(bus_pio, mem_sm, true);

    // I/O SM: JMP_PIN=22 (IORQ), SET pins=GPIO4
    pio_sm_config ci = z80bus_pio_io_program_get_default_config(off_io);
    sm_config_set_in_pins(&ci, 12);
    sm_config_set_set_pins(&ci, 4, 1);
    sm_config_set_jmp_pin(&ci, 22);
    sm_config_set_in_shift(&ci, false, true, 32);
    sm_config_set_out_shift(&ci, true, false, 32);
    sm_config_set_clkdiv(&ci, 1.0f);
    pio_sm_set_consecutive_pindirs(bus_pio, io_sm, 4, 1, false);
    pio_sm_set_consecutive_pindirs(bus_pio, io_sm, 12, 16, false);
    pio_sm_init(bus_pio, io_sm, off_io, &ci);
    pio_sm_set_enabled(bus_pio, io_sm, true);

    pio_active = true;
}
void z80bus_pio_stats(void) {
    printf("PIO bus: %lu cycles (%lu rd, %lu wr, %lu io, %lu refresh)\n",
           pio_cycles_total, pio_cycles_read,
           pio_cycles_write, pio_cycles_io, pio_cycles_refresh);
}
bool z80bus_pio_is_active(void) { return pio_active; }
void z80bus_pio_suspend(void) {
    pio_sm_set_enabled(bus_pio, mem_sm, false);
    pio_sm_set_enabled(bus_pio, io_sm, false);
}
void z80bus_pio_resume(void) {
    pio_sm_set_enabled(bus_pio, mem_sm, true);
    pio_sm_set_enabled(bus_pio, io_sm, true);
}
bool z80bus_pio_is_idle(void) {
    if (!pio_active) return true;
    return pio_sm_is_rx_fifo_empty(bus_pio, mem_sm) &&
           pio_sm_is_rx_fifo_empty(bus_pio, io_sm);
}
