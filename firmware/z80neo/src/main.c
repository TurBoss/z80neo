// main.c
// Z80NEO Firmware
// TurBoss 2026

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Pico 2
#include <pico/binary_info.h>
#include <pico/multicore.h>
#include <pico/stdlib.h>
#include <pico/time.h>

// Pico hardware
#include <hardware/adc.h>
#include <hardware/clocks.h>
#include <hardware/gpio.h>
#include <hardware/i2c.h>
#include <hardware/irq.h>
#include <hardware/pwm.h>
#include <hardware/spi.h>
#include <hardware/uart.h>
#include <hardware/vreg.h>
#include <hardware/psram.h>

// This firmware drives GPIO30-47 (RP2350B / BB48).  Building for the RP2350A
// (e.g. PICO_BOARD=pico2) caps NUM_BANK0_GPIOS at 30 and silently disables
// every gpio_*/PIO call above GPIO29.
#if defined(PICO_RP2350A) && PICO_RP2350A
#error "z80neo requires the RP2350B (48-GPIO) package: build with PICO_BOARD=z80neo_bb48"
#endif

// PIO clock generator (replaces PWM to free GPIO33/RESET)
#include "z80clock.pio.h"

// SD Card
#include "ff.h"
#include "tf_card.h"

// Screen
#include "ssd1306_i2c.h"

// Boot logo
#include "logo.h"
#include "bootlogo.h"

// Project modules
#include "display.h"
#include "cpm_disk.h"
#include "fuzix_disk.h"
#include "memory.h"
#include "midi.h"
#include "i2c_ee.h"
#include "spi_vdp.h"
#include "z80bus_pio.h"

float CPU_SPEED = 128000.0f;   // Z80 clock (PIO generator on GPIO32); INI may override
float CPU_DUTY  = 0.50f;       // Z80 clock HIGH-time fraction (0.05..0.95)

// Boot log: write to UART0 and mirror the same bytes to the NullVDP, blocking
// until they are sent, so the whole boot log appears on the VDP from the start.
static void boot_log(const char *s) {
    while (*s) {
        uint8_t c = (uint8_t)*s++;
        uart_putc(UART_ID, c);
        vdp_console_tx(c);
    }
    vdp_console_flush();
}

// Print the configured Z80 clock to the boot log (MHz + Hz).
static void log_cpu_clock(void) {
    uint32_t hz = (uint32_t)(CPU_SPEED + 0.5f);
    char b[80];
    snprintf(b, sizeof(b), "Z80 clock: %lu.%03lu MHz (%lu Hz, %lu%% duty)\r\n",
             (unsigned long)(hz / 1000000u),
             (unsigned long)((hz % 1000000u) / 1000u),
             (unsigned long)hz,
             (unsigned long)(CPU_DUTY * 100.0f + 0.5f));
    boot_log(b);
}

// Z80 clock SM state, kept at file scope so the generator can be re-applied
// after Z80NEO.INI is parsed (Z80_KHZ / Z80_DUTY) without a rebuild.
static PIO  clock_pio   = pio1;
static uint clock_sm    = 2;
static uint clock_off   = 0;
static bool clock_ready = false;

// Configure (or reconfigure) the Z80 clock generator on GPIO_PWM_SIG.
static void z80_clock_apply(void) {
    if (clock_ready)
        pio_sm_set_enabled(clock_pio, clock_sm, false);
    // PIO clock generator — clean 50% duty at any frequency.
    gpio_set_drive_strength(GPIO_PWM_SIG, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_slew_rate(GPIO_PWM_SIG, GPIO_SLEW_RATE_FAST);
    gpio_set_function(GPIO_PWM_SIG, GPIO_FUNC_PIO1);  // Z80 clock pin → PIO1
    // On RP2350B each PIO instance can only address 32 pins, either 0-31 or
    // 16-47.  Select the upper window when the clock pin is >= 32; this must
    // happen before pio_add_program/pio_sm_init (pio_sm_set_config does not set
    // the GPIO base for you), otherwise the SET base wraps and drives pin 0.
    pio_set_gpio_base(clock_pio, (GPIO_PWM_SIG >= 32) ? 16 : 0);
    if (!clock_ready) {
        clock_off = pio_add_program(clock_pio, &z80clock_program);
        clock_ready = true;
    }
    z80clock_program_init(clock_pio, clock_sm, clock_off, CPU_SPEED, CPU_DUTY, GPIO_PWM_SIG);
    log_cpu_clock();
}

// ===========================================================================
// main()
// ===========================================================================

// Enable the Cortex-M33 FPU (CP10/CP11).  The SDK is compiled with
// -march=armv8-m.main+fp, so parts of it (notably the pico_time alarm-pool IRQ
// handler) execute VFP instructions such as `vpush {d8}`.  The RP2350 RCP
// bring-up in crt0.S stores CPACR with only the CP7 bits, wiping the FPU
// enable that runtime_init sets, so the first VFP instruction traps as a
// UsageFault (CFSR UFSR NOCP) and the firmware dies in isr_hardfault, leaving
// the PIO holding Z80 WAIT.  Re-enable it before anything can use it.
void enable_fpu(void) {
    volatile uint32_t *cpacr = (volatile uint32_t *)0xE000ED88u;
    *cpacr |= (0xFu << 20);         // CP10/CP11: full access
    __asm__ volatile("dsb\nisb" ::: "memory");
}

int main(void) {
    enable_fpu();

    // UART
    uart_init(UART_ID, BAUD_RATE);
    sleep_ms(50);


    // PICO
    stdio_init_all();
    sleep_ms(100);

    // useful information for picotool
    bi_decl(bi_2pins_with_func(PICO_I2C_SDA_PIN, PICO_I2C_SCL_PIN, GPIO_FUNC_I2C));
    bi_decl(bi_program_description("z80neo firmware"));

    // I2C_1 SCREEN (recover the bus first in case a slave holds SCL low)
    i2c_bus_recover();
    gpio_set_function(PICO_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(PICO_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(PICO_I2C_SDA_PIN);
    gpio_pull_up(PICO_I2C_SCL_PIN);

    // STATUS LED
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

	    // Uart GPIO Init
	    gpio_pull_up(UART_RX_PIN);  // prevent floating RX → garbage null bytes
	    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
	    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    // Uart config
    int __unused actual = uart_set_baudrate(UART_ID, BAUD_RATE);
    uart_set_hw_flow(UART_ID, false, false);
    uart_set_format(UART_ID, DATA_BITS, STOP_BITS, PARITY);
    // Keep FIFO enabled (default) for hardware buffering —
    // our software TX buffer in uart_status_handler flushes into it safely.

    // Second UART (SIO channel B, Z80 ports 0x90/0x91) on GPIO38/39.
    uart1_init();
    spi_vdp_init();

    uart_status_handler();

    // Wipe any output left by the previous program/OS on the VDP / sdl_client
    // and force its console back to text mode before the banner.
    boot_console_reset();

    boot_log("\r\n");

    // Boot banner
    boot_log("\r\n");
//    boot_log("          @@@@   @@@@                             \r\n");
//    boot_log("         @    @ @   @@                            \r\n");
//    boot_log("  @@@@@@ @    @ @  @ @ @@@@@@@   @@@@@   @@@@@@   \r\n");
//    boot_log("     @@   @@@@  @ @  @  @@   @@ @@   @@ @@    @@  \r\n");
//    boot_log("    @@   @    @ @@   @  @@   @@ @@@@@@@ @@    @@  \r\n");
//    boot_log("   @@    @    @ @    @  @@   @@ @@      @@    @@  \r\n");
//    boot_log("  @@@@@@  @@@@   @@@@  @@@   @@  @@@@@@  @@@@@@   \r\n");
//    boot_log("                                                  \r\n");

    boot_log("\r\nz80neo - TurBoss 2026\r\n");

    // Graphical boot logo on the VDP / sdl_client (the ESC g bitmap protocol),
    // drawn on every boot before the SD is read and cleared back to text before
    // the Z80 / OS starts, so it is the first thing on screen.
    boot_logo_show();

    boot_log("\r\n");
    boot_log("Boot init\r\n");
    boot_log("Configure CPU clock\r\n");

    z80_clock_apply();
    boot_log("Initialize SD card\r\n");

    // SD card config
    pico_fatfs_spi_config_t fs_config = {
        spi1,             // SPI instance
        CLK_SLOW_DEFAULT, // slow clock
        CLK_FAST_DEFAULT, // fast clock
        PIN_SPI1_MISO,    // SPIx_RX
        PIN_SPI1_CS,      // SPIx_CS
        PIN_SPI1_SCK,     // SPIx_SCK
        PIN_SPI1_MOSI,    // SPIx_TX
        true              // use internal pullup
    };

    spi_configured = pico_fatfs_set_config(&fs_config);

    if (!spi_configured) {
        while (true) {
            gpio_put(LED_PIN, 1);
            for(volatile int d=0;d<150*30000;d++){};
            gpio_put(LED_PIN, 0);
            for(volatile int d=0;d<100*30000;d++){};
        }
    }

    // Reset GPIO
    gpio_init(RESET_OUT);
    gpio_set_dir(RESET_OUT, GPIO_OUT);
    gpio_put(RESET_OUT, true);
    gpio_set_function(RESET_OUT, GPIO_FUNC_SIO);
    reset_hold();

    // Z80 /INT (Fuzix hardware mod).  Active low; idle high.
    gpio_init(INT_OUT);
    gpio_set_dir(INT_OUT, GPIO_OUT);
    gpio_set_function(INT_OUT, GPIO_FUNC_SIO);
    gpio_put(INT_OUT, 1);

    // Init ADC keys
    boot_log("Init Keys\r\n");
    adc_init();
    adc_gpio_init(ADC_KEYS_INPUT);

    // Init I2C EEPROM emulation (port 0xD1)
    i2c_ee_init();

    // Init display
    boot_log("Init Display\r\n");
    SSD1306_init();
    calc_render_area_buflen(&frame_area);

    // zero the entire display
    {
        static uint8_t init_buf[SSD1306_BUF_LEN];
        memset(init_buf, 0, SSD1306_BUF_LEN);
        render(init_buf, &frame_area);
    }

    // Show logo
    show_logo();
    sleep_ms(DISPLAY_DELAY);

    // Boot info
    boot_screen();
    sleep_ms(DISPLAY_DELAY);
    sleep_ms(DISPLAY_DELAY);

    // Clear banks
    cur_bank = 0;
    for (uint8_t pgm = 0; pgm < MAX_BANKS; pgm++) {
        clear_bank(pgm);
    }

    clear_screen();

    boot_log("Read SD Card\r\n");
    WriteString(buf, 3, 0, "SD READ");
    render(buf, &frame_area);
    sleep_ms(DISPLAY_DELAY_SHORT);

    sd_read_init();

    // Apply the console baud rate read from Z80NEO.INI.  Everything printed up
    // to here (boot log/banner) is at the BAUD_RATE default; the Z80/CP/M
    // console runs at UART_BAUD from this point on.
    if (UART_BAUD != BAUD_RATE) {
        uart_tx_wait_blocking(UART_ID);
        uart_set_baudrate(UART_ID, UART_BAUD);
    }

    // Apply the second-UART settings read from Z80NEO.INI (UART1_BAUD /
    // UART1_FORMAT).  uart1_init() ran before the INI was parsed, so re-tune it
    // here; the native uart1term and CP/M both use this line afterwards.
    uart1_configure(UART1_BAUD, UART1_DATA_BITS, UART1_STOP_BITS, UART1_PARITY);

    // Apply the Z80 clock read from Z80NEO.INI (Z80_KHZ / Z80_DUTY).  The clock
    // ran at the compile-time default until here; the Z80 is still held in reset
    // so re-configuring the generator now is safe.
    z80_clock_apply();

    clear_screen();

    boot_log("Loading program...\r\n");
    WriteString(buf, 0, 0, "LOAD PROGS");
    render(buf, &frame_area);
    sleep_ms(DISPLAY_DELAY_SHORT);

    load_init_progs();
    // load_file() releases the Z80 reset when it finishes.  Re-assert it here
    // so the Z80 stays stopped until the PIO bus handler is fully initialized
    // and ready to service cycles — otherwise it runs freely for ~100ms with a
    // floating data bus and its PC derails before the first real fetch.
    reset_hold();
    sleep_ms(DISPLAY_DELAY_SHORT);
    boot_log("\r\nOk!\r\n");
    sleep_ms(DISPLAY_DELAY_SHORT);

    clear_screen();

    WriteString(buf, 3, 0, "SHOW INFO");
    render(buf, &frame_area);
    sleep_ms(DISPLAY_DELAY_SHORT);

    show_info();

#ifdef PSRAM_ENABLE
    boot_log("PSRAM...");
    psram_base = NULL;
    bool ok = psram_is_available();
    boot_log(ok ? "OK\r\n" : "NOPE\r\n");
    if (ok) {
        psram_base = (uint8_t *)(XIP_BASE + 0x1000000);
        for (uint8_t p = MAX_BANKS; p < TOTAL_BANKS; p++)
            clear_bank(p);
        boot_log("PSRAM OK\r\n");
    }
#endif

     	if (fuzix_mode) {
    	    boot_log(fuzix_disk_init()
    	                      ? "Fuzix disk: FUZIX.IMG mounted\r\n"
    	                      : "Fuzix disk: FUZIX.IMG not found\r\n");
    	} else if (cpm_disk_init()) {
    	    char cpm_msg[48];
    	    snprintf(cpm_msg, sizeof(cpm_msg), "CP/M disk: %u drive(s) mounted\r\n",
    	             (unsigned)cpm_disk_count());
    	    boot_log(cpm_msg);
    	} else {
    	    boot_log("No CP/M disk images found — disk I/O disabled\r\n");
    	}

    post_dump();

    // Init GPIO bus pins
    boot_log("Init GPIO pins\r\n");
    uint32_t gpio;
    for (gpio = BUS_GPIO_START; gpio <= BUS_GPIO_END; gpio++) {
        bus_mask |= (1 << gpio);
        gpio_init(gpio);
        gpio_set_function(gpio, GPIO_FUNC_SIO);
        gpio_set_dir(gpio, GPIO_IN);
    }

    // Address select 0-7
    gpio_init(SEL1_OUT);
    gpio_set_function(SEL1_OUT, GPIO_FUNC_SIO);
    gpio_set_dir(SEL1_OUT, GPIO_OUT);
    gpio_set_outover(SEL1_OUT, GPIO_OVERRIDE_NORMAL);
    gpio_put(SEL1_OUT, 1);

    // Address select 8-15
    gpio_init(SEL2_OUT);
    gpio_set_function(SEL2_OUT, GPIO_FUNC_SIO);
    gpio_set_dir(SEL2_OUT, GPIO_OUT);
    gpio_set_outover(SEL2_OUT, GPIO_OVERRIDE_NORMAL);
    gpio_put(SEL2_OUT, 1);

    // Data select 0-7 — HIGH DRIVE to overpower OE pullup on HC245
    gpio_init(SEL3_OUT);
    gpio_set_function(SEL3_OUT, GPIO_FUNC_SIO);
    gpio_set_dir(SEL3_OUT, GPIO_OUT);
    gpio_set_drive_strength(SEL3_OUT, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_outover(SEL3_OUT, GPIO_OVERRIDE_NORMAL);
    gpio_put(SEL3_OUT, 1);

    // Bus direction — HIGH DRIVE
    gpio_init(DIR1_OUT);
    gpio_set_function(DIR1_OUT, GPIO_FUNC_SIO);
    gpio_set_dir(DIR1_OUT, GPIO_OUT);
    gpio_set_drive_strength(DIR1_OUT, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_outover(DIR1_OUT, GPIO_OVERRIDE_NORMAL);
    gpio_put(DIR1_OUT, 0);

    gpio_init(DIR2_OUT);
    gpio_set_function(DIR2_OUT, GPIO_FUNC_SIO);
    gpio_set_dir(DIR2_OUT, GPIO_OUT);
    gpio_set_drive_strength(DIR2_OUT, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_outover(DIR2_OUT, GPIO_OVERRIDE_NORMAL);
    gpio_put(DIR2_OUT, 0);

    gpio_init(DIR3_OUT);
    gpio_set_function(DIR3_OUT, GPIO_FUNC_SIO);
    gpio_set_dir(DIR3_OUT, GPIO_OUT);
    gpio_set_drive_strength(DIR3_OUT, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_outover(DIR3_OUT, GPIO_OVERRIDE_NORMAL);
    gpio_put(DIR3_OUT, 0);

 	    // CPU MREQ — must be PIO0 function so the PIO bus SMs can read it via
	    // JMP PIN / WAIT.  gpio_get() still reads the pad via SIO regardless
	    // of function select.  Pull-up prevents false triggers during idle.
	    gpio_init(MREQ_INPUT);
	    gpio_set_function(MREQ_INPUT, GPIO_FUNC_PIO0);
	    gpio_set_dir(MREQ_INPUT, GPIO_IN);
	    gpio_pull_up(MREQ_INPUT);
	    gpio_set_inover(MREQ_INPUT, GPIO_OVERRIDE_NORMAL);

	    // CPU RD
	    gpio_init(RD_INPUT);
	    gpio_set_function(RD_INPUT, GPIO_FUNC_SIO);
	    gpio_set_dir(RD_INPUT, GPIO_IN);
	    gpio_pull_up(RD_INPUT);
	    gpio_set_inover(RD_INPUT, GPIO_OVERRIDE_NORMAL);

	    // CPU IORQ — PIO0 function so the io SM's WAIT GPIO 22 can read it.
	    // MUST have pull-up: glitches cause false I/O cycles.
	    gpio_init(IORQ_INPUT);
	    gpio_set_function(IORQ_INPUT, GPIO_FUNC_PIO0);
	    gpio_set_dir(IORQ_INPUT, GPIO_IN);
	    gpio_pull_up(IORQ_INPUT);
	    gpio_set_inover(IORQ_INPUT, GPIO_OVERRIDE_NORMAL);

	    // CPU WR
	    gpio_init(WR_INPUT);
	    gpio_set_function(WR_INPUT, GPIO_FUNC_SIO);
	    gpio_set_dir(WR_INPUT, GPIO_IN);
	    gpio_pull_up(WR_INPUT);
	    gpio_set_inover(WR_INPUT, GPIO_OVERRIDE_NORMAL);

    // Reset bus / address state
    m_adr     = 0;
    io_r_op   = 0;
    io_w_op   = 0;
    mem_r_op  = 0;
    mem_w_op  = 0;

    boot_log("Start Display handler\r\n");

    // Launch display loop on core 1
    multicore_launch_core1(display_loop);

    while (DEBUG_ADC) {
        for(volatile int d=0;d<1000*30000;d++){};
    }

    gpio_set_dir_masked(bus_mask, 0);

    gpio_put(SEL1_OUT, 1);
    gpio_put(SEL2_OUT, 1);
    gpio_put(SEL3_OUT, 1);

    // Ensure WAIT is HIGH before the PIO SMs take the pin
    // WAIT is open-drain (pull-up held HIGH, PIO drives LOW to assert)
    gpio_init(4); gpio_set_dir(4, GPIO_IN); gpio_pull_up(4);

    // UART0 RX — polled in main loop, no IRQ
    int UART_IRQ = UART_ID == uart0 ? UART0_IRQ : UART1_IRQ;
    irq_set_enabled(UART_IRQ, false);
    uart_set_irq_enables(UART_ID, false, false);

    boot_log("\r\nSystem UP!\a\r\n");
    sleep_ms(10);

    // SPI (NullVDP) status — printed here so it lands at the end of the boot log
    // instead of scrolling away under the banner/logo.
    {
        char vdpmsg[112];
        snprintf(vdpmsg, sizeof vdpmsg,
                 "VDP SPI: SCK=GPIO%d MOSI=GPIO%d MISO=GPIO%d CS=GPIO%d\r\n",
                 VDP_SPI_SCK_PIN, VDP_SPI_MOSI_PIN, VDP_SPI_MISO_PIN, VDP_SPI_CS_PIN);
        boot_log(vdpmsg);

        snprintf(vdpmsg, sizeof vdpmsg,
                 "VDP SPI probe: raw=0x%08X id=0x%02X status=0x%02X "
                 "(present=%u txrdy=%u rxavail=%u rxovf=%u)\r\n",
                 (unsigned)vdp_id_raw, (unsigned)(vdp_id_raw & 0xFF),
                 (unsigned)vdp_status,
                 (unsigned)((vdp_status >> 7) & 1), (unsigned)(vdp_status & 1),
                 (unsigned)((vdp_status >> 1) & 1), (unsigned)((vdp_status >> 2) & 1));
        boot_log(vdpmsg);

        snprintf(vdpmsg, sizeof vdpmsg,
                 "VDP SPI link: enabled=%u miso=pu%u/pd%u follow=%u (0=none 1=CS 2=SCK 3=MOSI)\r\n",
                 (unsigned)vdp_enabled, (unsigned)vdp_miso_pu,
                 (unsigned)vdp_miso_pd, (unsigned)vdp_miso_follows);
        boot_log(vdpmsg);

        // Slow (~2.5 kHz) ID read: separates a wiring/protocol fault from an
        // edge-rate / signal-integrity fault.
        {
            uint16_t slow = vdp_probe_slow();
            snprintf(vdpmsg, sizeof vdpmsg,
                     "VDP SPI slow ID: raw=0x%08X id=0x%02X\r\n",
                     (unsigned)slow, (unsigned)(slow & 0xFF));
            boot_log(vdpmsg);
        }

        // If the link is down, sweep all four SPI pins (works with the
        // `make ident-program` bitstream loaded) to map the wiring.
        if (!vdp_enabled) {
            static const uint32_t idpins[4] = {
                VDP_SPI_MISO_PIN, VDP_SPI_MOSI_PIN, VDP_SPI_SCK_PIN, VDP_SPI_CS_PIN
            };
            static const char *idname[4] = { "MISO", "MOSI", "SCK", "CS" };
            boot_log("VDP SPI ident (edges/2s):");
            for (int i = 0; i < 4; i++) {
                uint32_t e = vdp_probe_pin_edges(idpins[i]);
                const char *ball = (e == 0)   ? "static"
                                 : (e <= 2)   ? "N6"
                                 : (e <= 4)   ? "A11"
                                 : (e <= 9)   ? "B11"
                                 :              "D11";
                snprintf(vdpmsg, sizeof vdpmsg, " %s=GPIO%u:%u(%s)",
                         idname[i], (unsigned)idpins[i], (unsigned)e, ball);
                boot_log(vdpmsg);
            }
            boot_log("  [N6=SCK A11=MOSI B11=MISO D11=CS]\r\n");
        }
    }

    z80bus_pio_init((uint32_t)(CPU_SPEED / 1000.0f));
    // PIO SMs start with WAIT released (SET PINS, 1).
    // They will assert WAIT as soon as Z80 starts its first bus cycle.
    sleep_ms(10);
    // Wait a short time for MREQ to go HIGH (clean start).  Bounded so we can
    // never deadlock if the Z80 is already running/asserting MREQ.
    { int mq = 10000; while (!gpio_get(MREQ_INPUT) && --mq) tight_loop_contents(); }
    reset_release();
    system_up = true;

    uint32_t fuzix_last_tick = time_us_32();
    while (true) {
        // Fuzix clock.  The Z80 is slow (stalled on every bus cycle), and a
        // full timer interrupt costs tens of ms, so tick slowly (10 Hz) to
        // leave the kernel useful time between interrupts.
        if (fuzix_mode) {
            uint32_t now = time_us_32();
            if ((uint32_t)(now - fuzix_last_tick) >= 100000u) {
                fuzix_last_tick = now;
                fuzix_int_raise(FUZIX_IRQ_TIMER);
            }
        }
        if (disabled) {
            z80bus_pio_suspend();
            pio_sm_set_enabled(clock_pio, clock_sm, false);
            reset_hold(); gpio_put(LED_PIN, 1);
            confirmed = true;
            while (disabled) {};
            pio_sm_set_enabled(clock_pio, clock_sm, true);
            gpio_put(LED_PIN, 0); reset_release();
            z80bus_pio_resume();
        }
        z80bus_pio_poll();
        // Timing-trace dump/reset requested from gdb.
        if (z80bus_diag_reset) { z80bus_diag_reset = false; z80bus_pio_diag_reset(); }
        if (z80bus_diag_dump)  { z80bus_diag_dump = false;  z80bus_pio_dump(); }
        // Flush 1 UART byte per poll
        if (tx_count > 0 && uart_is_writable(UART_ID)) {
            echo_track_tx(tx_buffer[tx_tail]);
            uart_putc(UART_ID, tx_buffer[tx_tail]);
            vdp_console_tx(tx_buffer[tx_tail]);   // mirror the console to the NullVDP
            tx_tail = (tx_tail + 1) % UART_TX_BUF_SIZE;
            tx_count--;
            serial_status_1 = (tx_count < UART_TX_BUF_SIZE) ? (serial_status_1 | 0x02) : (serial_status_1 & ~0x02);
        }
        // Poll UART RX into the software buffer.  Do NOT drop bytes while TX is
        // in flight: the CCP echoes every typed character, and discarding RX
        // during that echo throws away the user's next character (fast typing /
        // paste loses input).  The terminal must run with local echo OFF.
        uart_rx_poll();

        // Second UART: flush TX and drain RX (Z80 ports 0x90/0x91).
        uart1_poll();

        // NullVDP SPI link: mirror console output, pick up keyboard input.
        spi_vdp_poll();

        // MIDI channel monitor: repaint changed rows at a throttled rate.
        midi_panel_poll();
    }
}
