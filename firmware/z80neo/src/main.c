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

// Project modules
#include "display.h"
#include "cpm_disk.h"
#include "memory.h"
#include "i2c_ee.h"
#include "z80bus_pio.h"

float CPU_SPEED = 128000.0f;   // Z80 clock (PIO generator on GPIO32); INI may override
float CPU_DUTY  = 0.50f;       // Z80 clock HIGH-time fraction (0.05..0.95)

// Print the configured Z80 clock to the boot log (MHz + Hz).
static void log_cpu_clock(void) {
    uint32_t hz = (uint32_t)(CPU_SPEED + 0.5f);
    char b[80];
    snprintf(b, sizeof(b), "Z80 clock: %lu.%03lu MHz (%lu Hz, %lu%% duty)\r\n",
             (unsigned long)(hz / 1000000u),
             (unsigned long)((hz % 1000000u) / 1000u),
             (unsigned long)hz,
             (unsigned long)(CPU_DUTY * 100.0f + 0.5f));
    uart_puts(UART_ID, b);
}

// ===========================================================================
// main()
// ===========================================================================

int main(void) {
    // UART
    uart_init(UART_ID, BAUD_RATE);
    sleep_ms(50);


    // PICO
    stdio_init_all();
    sleep_ms(100);

    // useful information for picotool
    bi_decl(bi_2pins_with_func(PICO_I2C_SDA_PIN, PICO_I2C_SCL_PIN, GPIO_FUNC_I2C));
    bi_decl(bi_program_description("z80neo firmware"));

    // I2C_1 SCREEN
    i2c_init(PICO_I2C_INSTANCE, SSD1306_I2C_CLK * 1000);
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

    uart_status_handler();

    uart_puts(UART_ID, "\r\n");

    // Boot banner
    uart_puts(UART_ID, "\r\n");
    uart_puts(UART_ID, "          @@@@   @@@@                             \r\n");
    uart_puts(UART_ID, "         @    @ @   @@                            \r\n");
    uart_puts(UART_ID, "  @@@@@@ @    @ @  @ @ @@@@@@@   @@@@@   @@@@@@   \r\n");
    uart_puts(UART_ID, "     @@   @@@@  @ @  @  @@   @@ @@   @@ @@    @@  \r\n");
    uart_puts(UART_ID, "    @@   @    @ @@   @  @@   @@ @@@@@@@ @@    @@  \r\n");
    uart_puts(UART_ID, "   @@    @    @ @    @  @@   @@ @@      @@    @@  \r\n");
    uart_puts(UART_ID, "  @@@@@@  @@@@   @@@@  @@@   @@  @@@@@@  @@@@@@   \r\n");
    uart_puts(UART_ID, "                                                  \r\n");
    uart_puts(UART_ID, "\r\nz80neo - TurBoss 2026\r\n");
    uart_puts(UART_ID, "\r\n");
    uart_puts(UART_ID, "Boot init\r\n");
    uart_puts(UART_ID, "Configure CPU clock\r\n");

    // PIO clock generator — clean 50% duty at any frequency
    gpio_set_drive_strength(GPIO_PWM_SIG, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_slew_rate(GPIO_PWM_SIG, GPIO_SLEW_RATE_FAST);
    gpio_set_function(GPIO_PWM_SIG, GPIO_FUNC_PIO1);  // Z80 clock pin → PIO1
    PIO clock_pio = pio1;
    uint clock_sm = 2;
    // On RP2350B each PIO instance can only address 32 pins, either 0-31 or
    // 16-47.  Select the upper window when the clock pin is >= 32; this must
    // happen before pio_add_program/pio_sm_init (pio_sm_set_config does not set
    // the GPIO base for you), otherwise the SET base wraps and drives pin 0.
    pio_set_gpio_base(clock_pio, (GPIO_PWM_SIG >= 32) ? 16 : 0);
    uint clock_off = pio_add_program(clock_pio, &z80clock_program);
    z80clock_program_init(clock_pio, clock_sm, clock_off, CPU_SPEED, CPU_DUTY, GPIO_PWM_SIG);
    log_cpu_clock();
    uart_puts(UART_ID, "Initialize SD card\r\n");

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

    // Init ADC keys
    uart_puts(UART_ID, "Init Keys\r\n");
    adc_init();
    adc_gpio_init(ADC_KEYS_INPUT);

    // Init I2C EEPROM emulation (port 0xD1)
    i2c_ee_init();

    // Init display
    uart_puts(UART_ID, "Init Display\r\n");
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
    sleep_ms(DISPLAY_DELAY_LONG);
    sleep_ms(DISPLAY_DELAY_LONG);

    // Boot info
    boot_screen();
    sleep_ms(DISPLAY_DELAY_LONG);
    sleep_ms(DISPLAY_DELAY_LONG);
    sleep_ms(DISPLAY_DELAY_LONG);
    sleep_ms(DISPLAY_DELAY_LONG);

    // Clear banks
    cur_bank = 0;
    for (uint8_t pgm = 0; pgm < MAX_BANKS; pgm++) {
        clear_bank(pgm);
    }

    clear_screen();

    uart_puts(UART_ID, "Read SD Card\r\n");
    WriteString(buf, 3, 0, "SD READ");
    render(buf, &frame_area);
    sleep_ms(DISPLAY_DELAY_LONG);

    sd_read_init();

    clear_screen();

    uart_puts(UART_ID, "Loading program...\r\n");
    WriteString(buf, 0, 0, "LOAD PROGS");
    render(buf, &frame_area);
    sleep_ms(DISPLAY_DELAY_LONG);

    load_init_progs();
    // load_file() releases the Z80 reset when it finishes.  Re-assert it here
    // so the Z80 stays stopped until the PIO bus handler is fully initialized
    // and ready to service cycles — otherwise it runs freely for ~100ms with a
    // floating data bus and its PC derails before the first real fetch.
    reset_hold();
    sleep_ms(DISPLAY_DELAY_LONG);
    uart_puts(UART_ID, "\r\nOk!\r\n");
    sleep_ms(DISPLAY_DELAY_LONG);

    clear_screen();

    WriteString(buf, 3, 0, "SHOW INFO");
    render(buf, &frame_area);
    sleep_ms(DISPLAY_DELAY_LONG);

    show_info();

#ifdef PSRAM_ENABLE
    uart_puts(UART_ID, "PSRAM...");
    psram_base = NULL;
    bool ok = psram_is_available();
    uart_puts(UART_ID, ok ? "OK\r\n" : "NOPE\r\n");
    if (ok) {
        psram_base = (uint8_t *)(XIP_BASE + 0x1000000);
        for (uint8_t p = MAX_BANKS; p < TOTAL_BANKS; p++)
            clear_bank(p);
        uart_puts(UART_ID, "PSRAM OK\r\n");
    }
#endif

    	if (cpm_disk_init()) {
    	    char cpm_msg[48];
    	    snprintf(cpm_msg, sizeof(cpm_msg), "CP/M disk: %u drive(s) mounted\r\n",
    	             (unsigned)cpm_disk_count());
    	    uart_puts(UART_ID, cpm_msg);
    	} else {
    	    uart_puts(UART_ID, "No CP/M disk images found — disk I/O disabled\r\n");
    	}

    post_dump();

    // Init GPIO bus pins
    uart_puts(UART_ID, "Init GPIO pins\r\n");
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

    uart_puts(UART_ID, "Start Display handler\r\n");

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

    uart_puts(UART_ID, "\r\nSystem UP!\r\n");
    sleep_ms(10);

    z80bus_pio_init((uint32_t)(CPU_SPEED / 1000.0f));
    // PIO SMs start with WAIT released (SET PINS, 1).
    // They will assert WAIT as soon as Z80 starts its first bus cycle.
    sleep_ms(10);
    // Wait a short time for MREQ to go HIGH (clean start).  Bounded so we can
    // never deadlock if the Z80 is already running/asserting MREQ.
    { int mq = 10000; while (!gpio_get(MREQ_INPUT) && --mq) tight_loop_contents(); }
    reset_release();
    system_up = true;

    while (true) {
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
            tx_tail = (tx_tail + 1) % UART_TX_BUF_SIZE;
            tx_count--;
            serial_status_1 = (tx_count < UART_TX_BUF_SIZE) ? (serial_status_1 | 0x02) : (serial_status_1 & ~0x02);
        }
        // Poll UART RX into the software buffer.  Do NOT drop bytes while TX is
        // in flight: the CCP echoes every typed character, and discarding RX
        // during that echo throws away the user's next character (fast typing /
        // paste loses input).  The terminal must run with local echo OFF.
        uart_rx_poll();
    }
}
