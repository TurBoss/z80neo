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

// SD Card
#include "ff.h"
#include "tf_card.h"

// Utils
#include "utils.h"

// USB Serial
// #include "cdc.h"

// Screen
#include "ssd1306_i2c.h"

// Boot logo
#include "logo.h"

// Project modules
#include "display.h"
#include "memory.h"

// ===========================================================================
// main()
// ===========================================================================

int main(void) {
    // UART
    uart_init(UART_ID, BAUD_RATE);

    // USB
    // usb_cdc_init();

    // PICO
    stdio_init_all();
    sleep_ms(500);

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
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    // Uart config
    int __unused actual = uart_set_baudrate(UART_ID, BAUD_RATE);
    uart_set_hw_flow(UART_ID, false, false);
    uart_set_format(UART_ID, DATA_BITS, STOP_BITS, PARITY);
    // Keep FIFO enabled (default) for hardware buffering —
    // our software TX buffer in uart_status_handler flushes into it safely.

    uart_status_handler();

    // Boot banner
    uart_puts(UART_ID, "\r\n");
    uart_puts(UART_ID, "          ████   ████                             \r\n");
    uart_puts(UART_ID, "         █░░░ █ █░░░██                            \r\n");
    uart_puts(UART_ID, "  ██████░█   ░█░█  █░█ ███████   █████   ██████   \r\n");
    uart_puts(UART_ID, " ░░░░██ ░ ████ ░█ █ ░█░░██░░░██ ██░░░██ ██░░░░██  \r\n");
    uart_puts(UART_ID, "    ██   █░░░ █░██  ░█ ░██  ░██░███████░██   ░██  \r\n");
    uart_puts(UART_ID, "   ██   ░█   ░█░█   ░█ ░██  ░██░██░░░░ ░██   ░██  \r\n");
    uart_puts(UART_ID, "  ██████░ ████ ░ ████  ███  ░██░░██████░░██████   \r\n");
    uart_puts(UART_ID, " ░░░░░░  ░░░░   ░░░░  ░░░   ░░  ░░░░░░  ░░░░░░    \r\n");
    uart_puts(UART_ID, "\r\n");
    uart_puts(UART_ID, "z80neo - TurBoss 2026\r\n");
    uart_puts(UART_ID, "\r\n");
    uart_puts(UART_ID, "Boot init\r\n");
    uart_puts(UART_ID, "Configure CPU clock\r\n");

    // Configure GPIO PIN for PWM
    gpio_set_function(GPIO_PWM_SIG, GPIO_FUNC_PWM);
    uint slice_num   = pwm_gpio_to_slice_num(GPIO_PWM_SIG);
    uint channel_num = pwm_gpio_to_channel(GPIO_PWM_SIG);

    // Calculate clock divider
    uint32_t target_wrap  = PWM_WRAP;
    float frequency_hz    = 10000.0f;   // Hz Z80 clock
    float system_clock    = clock_get_hz(clk_sys);

    float clock_divider = system_clock / (frequency_hz * (target_wrap + 1));

    if (clock_divider < 10.0f) {
        clock_divider = 10.0f;
        target_wrap   = (uint32_t)(system_clock / (frequency_hz * clock_divider)) - 1;
    } else if (clock_divider > 255.0f) {
        clock_divider = 255.0f;
        target_wrap   = (uint32_t)(system_clock / (frequency_hz * clock_divider)) - 1;
        if (target_wrap > 65535)
            target_wrap = 65535;
    }

    int duty_cycle = target_wrap * 0.50;   // 50% duty cycle

    // Configure PWM
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, clock_divider);
    pwm_config_set_wrap(&config, target_wrap);
    pwm_init(slice_num, &config, true);

    // Configure PWM IRQ
    pwm_clear_irq(slice_num);
    pwm_set_irq_enabled(slice_num, true);
    irq_set_exclusive_handler(PWM_IRQ_WRAP, pwm_irq_handler);

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
            sleep_ms(150);
            gpio_put(LED_PIN, 0);
            sleep_ms(100);
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
    for (uint8_t pgm = 0; pgm <= MAX_BANKS; pgm++) {
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
    sleep_ms(DISPLAY_DELAY_LONG);
    uart_puts(UART_ID, "\r\nOk!\r\n");
    sleep_ms(DISPLAY_DELAY_LONG);

    clear_screen();

    WriteString(buf, 3, 0, "SHOW INFO");
    render(buf, &frame_area);
    sleep_ms(DISPLAY_DELAY_LONG);

    show_info();

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

    // Data select 0-7
    gpio_init(SEL3_OUT);
    gpio_set_function(SEL3_OUT, GPIO_FUNC_SIO);
    gpio_set_dir(SEL3_OUT, GPIO_OUT);
    gpio_set_outover(SEL3_OUT, GPIO_OVERRIDE_NORMAL);
    gpio_put(SEL3_OUT, 1);

    // Bus direction
    gpio_init(DIR1_OUT);
    gpio_set_function(DIR1_OUT, GPIO_FUNC_SIO);
    gpio_set_dir(DIR1_OUT, GPIO_OUT);
    gpio_set_outover(DIR1_OUT, GPIO_OVERRIDE_NORMAL);
    gpio_put(DIR1_OUT, 1);

    gpio_init(DIR2_OUT);
    gpio_set_function(DIR2_OUT, GPIO_FUNC_SIO);
    gpio_set_dir(DIR2_OUT, GPIO_OUT);
    gpio_set_outover(DIR2_OUT, GPIO_OVERRIDE_NORMAL);
    gpio_put(DIR2_OUT, 1);

    gpio_init(DIR3_OUT);
    gpio_set_function(DIR3_OUT, GPIO_FUNC_SIO);
    gpio_set_dir(DIR3_OUT, GPIO_OUT);
    gpio_set_outover(DIR3_OUT, GPIO_OVERRIDE_NORMAL);
    gpio_put(DIR3_OUT, 1);

    // CPU MREQ
    gpio_init(MREQ_INPUT);
    gpio_set_function(MREQ_INPUT, GPIO_FUNC_SIO);
    gpio_set_dir(MREQ_INPUT, GPIO_IN);
    gpio_set_inover(MREQ_INPUT, GPIO_OVERRIDE_NORMAL);

    // CPU RD
    gpio_init(RD_INPUT);
    gpio_set_function(RD_INPUT, GPIO_FUNC_SIO);
    gpio_set_dir(RD_INPUT, GPIO_IN);
    gpio_set_inover(RD_INPUT, GPIO_OVERRIDE_NORMAL);

    // CPU IORQ
    gpio_init(IORQ_INPUT);
    gpio_set_function(IORQ_INPUT, GPIO_FUNC_SIO);
    gpio_set_dir(IORQ_INPUT, GPIO_IN);
    gpio_set_inover(IORQ_INPUT, GPIO_OVERRIDE_NORMAL);

    // CPU WR
    gpio_init(WR_INPUT);
    gpio_set_function(WR_INPUT, GPIO_FUNC_SIO);
    gpio_set_dir(WR_INPUT, GPIO_IN);
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
        sleep_ms(1000);
    }

    gpio_set_dir_masked(bus_mask, 0);

    gpio_put(SEL1_OUT, 1);
    gpio_put(SEL2_OUT, 1);
    gpio_put(SEL3_OUT, 1);

    // UART0 RX IRQ
    int UART_IRQ = UART_ID == uart0 ? UART0_IRQ : UART1_IRQ;
    irq_set_exclusive_handler(UART_IRQ, on_uart_rx);
    irq_set_enabled(UART_IRQ, true);
    uart_set_irq_enables(UART_ID, true, false);

    // Enable CPU clock (PWM)
    uart_puts(UART_ID, "Start CPU clock\r\n");
    pwm_set_chan_level(slice_num, channel_num, duty_cycle);
    uart_puts(UART_ID, "\r\nSystem UP!\r\n");

    irq_set_enabled(PWM_IRQ_WRAP, true);

    reset_release();

    // Main loop
    while (true) {
        if (disabled) {
            reset_hold();
            gpio_put(LED_PIN, 1);

            // Stop PWM
            pwm_set_chan_level(slice_num, channel_num, 0);
            irq_set_enabled(slice_num, false);

            t_clock = 0;
            m_clock = 0;

            confirmed = true;

            while (disabled) {
            };

            // Re-enable PWM
            pwm_set_chan_level(slice_num, channel_num, duty_cycle);
            irq_set_enabled(slice_num, true);

            gpio_put(LED_PIN, 0);
            reset_release();
        }

        uart_status_handler();
        tight_loop_contents();
        // cdc_task();
    }
}
