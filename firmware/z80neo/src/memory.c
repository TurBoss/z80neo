// memory.c — GPIO-based Z80 bus handler (PWM ISR)
// Z80NEO Firmware — Memory, bus, & file I/O module
// TurBoss 2026

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pico/stdlib.h>
#include <pico/time.h>
#include <hardware/gpio.h>
#include <hardware/irq.h>
#include <hardware/pwm.h>
#include <hardware/uart.h>
#include <hardware/flash.h>
#include <hardware/sync.h>

#include "ff.h"
#include "tf_card.h"

#include "display.h"
#include "memory.h"
#include "utils.h"
#include "i2c_ee.h"
#include "flash_disk.h"

// ===========================================================================
// Globals
// ===========================================================================

uint8_t ram[MAX_BANKS][(uint16_t)RAM_SIZE] = {};
uint8_t mmuram[(uint16_t)RAM_SIZE]        = {};
uint8_t sdram[(uint16_t)SD_RAM_SIZE]      = {};

volatile uint8_t cur_bank = 0;

uint16_t pc     = 0;
uint8_t  opcode = 0;

volatile uint8_t t_clock = 0;
volatile uint8_t m_clock = 0;

uint8_t next_oclock = 0;
uint8_t next_mclock = 0;
uint8_t next_dclock = 0;

volatile uint8_t  low_adr  = 0x00;
volatile uint8_t  high_adr = 0x00;
volatile uint16_t m_adr    = 0x0000;

volatile uint8_t io_r_op     = 0x00;
volatile uint8_t io_w_op     = 0x00;
volatile uint8_t opcode_r_op = 0x00;
volatile uint8_t mem_r_op    = 0x00;
volatile uint8_t mem_w_op    = 0x00;

char    rx_buffer[UART_BUF_SIZE];
uint8_t rx_count          = 0;
uint8_t rx_index          = 0;
uint8_t rx_read           = 0;
bool    rx_data_available = false;

volatile uint8_t serial_status_1 = 0x02;
volatile uint8_t serial_status_2 = 0x00;

#define UART_TX_BUF_SIZE 1024
uint8_t  tx_buffer[UART_TX_BUF_SIZE];
uint16_t tx_head = 0;
uint16_t tx_tail = 0;
uint16_t tx_count = 0;

uint32_t d_adr = 0;
uint32_t dr_op = 0;
uint32_t dw_op = 0;
uint32_t io_op = 0;

bool mreq      = true;
bool iorq      = true;
bool clk_level = true;
bool rd        = true;
bool wr        = true;
bool mreq_status = false;
bool iorq_status = false;

uint32_t bus_mask = 0;

volatile bool disabled  = false;
volatile bool read      = false;
volatile bool written   = false;

volatile uint16_t CANCEL2_ADC = 0xFFF;
volatile uint16_t CANCEL_ADC  = 0xBFF;
volatile uint16_t OK_ADC      = 0x7FF;
volatile uint16_t BACK_ADC    = 0x5FF;
volatile uint16_t DOWN_ADC    = 0x2FF;
volatile uint16_t UP_ADC      = 0x0FF;

volatile bool DEBUG_ADC = false;

bool spi_configured;
uint slice;

char    MACHINE[FILE_LENGTH] = "Z80 CPU";
char    BANK_PROG[4][FILE_LENGTH];
uint8_t in_bytes[1024];
char    log_buf[32];

// ===========================================================================
// PIO + DMA
// ===========================================================================

// ===========================================================================
// Helpers
// ===========================================================================

unsigned char decode_hex(char c) {
    if (c >= 65 && c <= 70)      return c - 65 + 10;
    else if (c >= 97 && c <= 102) return c - 97 + 10;
    else if (c >= 48 && c <= 67)  return c - 48;
    else                          return -1;
}

// ===========================================================================
// Forward declarations
// ===========================================================================

static inline uint8_t mmu_read(uint16_t addr);
static inline void mmu_write(uint16_t addr, uint8_t data);

// ===========================================================================
// Bank management
// ===========================================================================

void clear_bank(uint8_t bank) {
    for (uint16_t adr = 0; adr < RAM_SIZE; adr++)
        ram[bank][adr] = 0;
    memset(BANK_PROG[bank], 0, FILE_LENGTH);
}

// ===========================================================================
// MMU
// ===========================================================================

#define MMU_PHYS_START   0x20
#define MMU_PHYS_PAGES   32

static uint8_t mmu_page[4] = {
    MMU_PHYS_START + 0,
    MMU_PHYS_START + 1,
    MMU_PHYS_START + 2,
    MMU_PHYS_START + 3
};

static inline uint8_t phys_to_bank(uint8_t phys_page) {
    return (phys_page - MMU_PHYS_START) % MAX_BANKS;
}

static inline uint8_t mmu_read(uint16_t addr) {
    uint8_t page = (addr >> 14) & 0x03;
    return ram[phys_to_bank(mmu_page[page])][addr & 0x3FFF];
}

// After load_init_progs, lock RST vectors against accidental Z80 writes
static volatile bool rst_locked = false;

static inline void mmu_write(uint16_t addr, uint8_t data) {
    // z80neo: write-protect the first 4 bytes (RST 00 vector) AFTER loading,
    // to prevent corruption during restart or errant writes during execution.
    if (rst_locked && addr < 4 && mmu_page[(addr >> 14) & 0x03] == 0x20) return;
    uint8_t page = (addr >> 14) & 0x03;
    ram[phys_to_bank(mmu_page[page])][addr & 0x3FFF] = data;
}

// ===========================================================================
// I/O device handler — UART + MMU ports (handled by GPIO IRQ on IORQ)
// ===========================================================================

static uint8_t io_port  = 0;

static uint8_t io_read_port(uint8_t port) {
    switch (port) {
    case SERIAL_PORT_1: {
        uint8_t ch = read_uart_char();
        serial_status_1 = (rx_data_available) ? (serial_status_1 | 0x01) : (serial_status_1 & ~0x01);
        return ch;
    }
    case 0xD1: return i2c_ee_read();
    case 0xE0: return fd_read_port(0xE0);
    case 0xE1: case 0xE2: case 0xE3: return 0x00;
    case SERIAL_STATUS_1:
        serial_status_1 = (rx_data_available) ? (serial_status_1 | 0x01) : (serial_status_1 & ~0x01);
        return serial_status_1;
    case 0xF1: return mmu_page[0];
    case 0xF3: return mmu_page[1];
    case 0xF5: return mmu_page[2];
    case 0xF7: return mmu_page[3];
    default: return 0x00;
    }
}

static void io_write_port(uint8_t port, uint8_t data) {
    switch (port) {
    case SERIAL_PORT_1:
        // Non-blocking: buffer and flush what we can
        if (tx_count < UART_TX_BUF_SIZE) {
            tx_buffer[tx_head++] = data;
            if (tx_head >= UART_TX_BUF_SIZE) tx_head = 0;
            tx_count++;
        }
        // Flush up to 4 bytes (non-blocking, rest in uart_status_handler)
        for (int f = 0; f < 4 && tx_count > 0 && uart_is_writable(UART_ID); f++) {
            uart_putc(UART_ID, tx_buffer[tx_tail++]);
            if (tx_tail >= UART_TX_BUF_SIZE) tx_tail = 0;
            tx_count--;
        }
        serial_status_1 = (tx_count < UART_TX_BUF_SIZE) ? (serial_status_1 | 0x02) : (serial_status_1 & ~0x02);
        break;
    case 0xF1: mmu_page[0] = data; break;
    case 0xF3: mmu_page[1] = data; break;
    case 0xF5: mmu_page[2] = data; break;
    case 0xF7: mmu_page[3] = data; break;
    case 0xD1: i2c_ee_write(data); return;
    case 0xE0: case 0xE1: case 0xE2: case 0xE3: fd_write_port(port, data); return;
    case SERIAL_STATUS_1:
    default: break;
    }
}

// ===========================================================================
// Reset
// ===========================================================================

void reset_release(void) {
    gpio_set_dir(RESET_OUT, GPIO_OUT);
    gpio_put(RESET_OUT, true);
}

void reset_hold(void) {
    gpio_set_dir(RESET_OUT, GPIO_OUT);
    gpio_put(RESET_OUT, false);
    t_clock = 0;
    m_clock = 0;
}

// ===========================================================================
// Bus control (used by IO handler only — PIO handles memory cycles)
// ===========================================================================

void set_bus_dir(int direction) {
    if (direction) gpio_set_dir_masked(bus_mask, bus_mask);
    else           gpio_set_dir_masked(bus_mask, 0);
}

void bus_manager(int value) {
    if (value == 0) {  // OFF
        gpio_put(DIR1_OUT, 1); gpio_put(DIR2_OUT, 1); gpio_put(DIR3_OUT, 1);
        gpio_put(SEL1_OUT, 1); gpio_put(SEL2_OUT, 1); gpio_put(SEL3_OUT, 1);
    } else if (value == 1) {  // ADDR LOW
        gpio_put(DIR1_OUT, 0); gpio_put(DIR2_OUT, 1); gpio_put(DIR3_OUT, 1);
        gpio_put(SEL1_OUT, 0); gpio_put(SEL2_OUT, 1); gpio_put(SEL3_OUT, 1);
    } else if (value == 2) {  // ADDR HIGH
        gpio_put(DIR1_OUT, 1); gpio_put(DIR2_OUT, 0); gpio_put(DIR3_OUT, 1);
        gpio_put(SEL1_OUT, 1); gpio_put(SEL2_OUT, 0); gpio_put(SEL3_OUT, 1);
    } else if (value == 3) {  // DATA
        gpio_put(DIR1_OUT, 1); gpio_put(DIR2_OUT, 1); gpio_put(DIR3_OUT, 1);
        gpio_put(SEL1_OUT, 1); gpio_put(SEL2_OUT, 1); gpio_put(SEL3_OUT, 0);
    }
}

// ===========================================================================
// UART
// ===========================================================================

void on_uart_rx(void) {
    char rx_char = uart_getc(UART_ID);
    rx_buffer[rx_index] = rx_char;
    rx_index = (rx_index + 1) % UART_BUF_SIZE;
    rx_count++;
    rx_data_available = true;
}

static inline uint8_t circ_next(uint8_t idx) {
    return (idx + 1) % UART_BUF_SIZE;
}

char read_uart_char(void) {
    if (rx_count > 0) {
        char ch = rx_buffer[rx_read];
        rx_read = circ_next(rx_read);
        rx_count--;
        return ch;
    }
    rx_data_available = false;
    return '\0';
}

void uart_status_handler(void) {
    while (tx_count > 0 && uart_is_writable(UART_ID)) {
        uart_putc(UART_ID, tx_buffer[tx_tail]);
        tx_tail = (tx_tail + 1) % UART_TX_BUF_SIZE;
        tx_count--;
    }
    serial_status_1 = (uart_is_writable(UART_ID) && tx_count < UART_TX_BUF_SIZE)
                      ? (serial_status_1 | 0x02) : (serial_status_1 & ~0x02);
    if (rx_data_available) serial_status_1 |= 0x01;
    else                   serial_status_1 &= ~0x01;
}

// ===========================================================================
// Bus poll — main-loop based Z80 bus handler
// ===========================================================================

static bool bus_active = false;

void bus_poll(void) {
    if (!gpio_get(IORQ_INPUT)) {
        if (bus_active) return;
        bus_active = true;
        io_op++;

        // Force all muxes off FIRST — clear residual state from memory handler
        gpio_put(SEL3_OUT, 1); gpio_put(SEL1_OUT, 1); gpio_put(SEL2_OUT, 1);
        gpio_set_dir_masked(bus_mask, 0);
        busy_wait_us_32(2);

        // Wait for WR or RD FIRST, then read port
        int timeout = 5000;
        while (gpio_get(WR_INPUT) && gpio_get(RD_INPUT) && --timeout) tight_loop_contents();

        // Read port using same dual-mux approach as memory addresses
        gpio_put(SEL2_OUT, 1);  // ensure SEL2 off
        gpio_put(SEL1_OUT, 0);  // SEL1 = low byte
        busy_wait_us_32(2);
        uint8_t pl = ((gpio_get_all() & bus_mask) >> BUS_GPIO_START) & 0xFF;
        uint32_t mask = (1u << SEL1_OUT) | (1u << SEL2_OUT);
        gpio_put_masked(mask, (1u << SEL1_OUT));  // SEL1=1, SEL2=0
        busy_wait_us_32(2);
        uint8_t ph = ((gpio_get_all() & bus_mask) >> BUS_GPIO_START) & 0xFF;
        gpio_put(SEL2_OUT, 1);  // muxes off
        uint8_t port = pl;
        // HW: A0 stuck high → both 0xF2 and 0xF3 read as 0xF3. Map to 0xF3.

        if (!gpio_get(WR_INPUT)) {
            gpio_put(SEL3_OUT, 0); gpio_put(DIR3_OUT, 0);
            busy_wait_us_32(3);
            io_w_op = ((gpio_get_all() & bus_mask) >> BUS_GPIO_START) & 0xFF;
            gpio_put(SEL3_OUT, 1);
            bus_active = false;  // allow new cycles while UART may block
            io_write_port(port, io_w_op);
            while (!gpio_get(IORQ_INPUT)) tight_loop_contents();
            // Critical: spin-wait for next MREQ — Z80 starts memory cycle
            // immediately after IORQ, and we MUST catch it or Z80 reads garbage
            while (gpio_get(MREQ_INPUT)) tight_loop_contents();
            goto handle_mem;
        } else if (!gpio_get(RD_INPUT)) {
            io_r_op = io_read_port(port);
            gpio_put(SEL3_OUT, 0); gpio_put(DIR3_OUT, 1);
            set_bus_dir(1);
            gpio_put_masked(bus_mask, (uint32_t)io_r_op << BUS_GPIO_START);
            while (!gpio_get(IORQ_INPUT)) tight_loop_contents();
            gpio_set_dir_masked(bus_mask, 0);
            gpio_put(SEL3_OUT, 1);
        }
        bus_active = false;
        return;
    }

    // Label: memory cycle detected during IORQ wait. Clean up IO state and handle it.
handle_mem:
    bus_active = false;
    gpio_set_dir_masked(bus_mask, 0);
    gpio_put(SEL3_OUT, 1);
    // Fall through to MREQ handler

    if (!gpio_get(MREQ_INPUT)) {
        if (bus_active) return;
        if (gpio_get(RD_INPUT) && gpio_get(WR_INPUT)) return;
        bus_active = true;


        // Read address — write both SEL pins atomically via SIO
        gpio_put(SEL2_OUT, 1);  // ensure SEL2 off first
        gpio_put(SEL1_OUT, 0);  // SEL1 = low byte
        busy_wait_us_32(2);     // 2us settle
        uint8_t al = ((gpio_get_all() & bus_mask) >> BUS_GPIO_START) & 0xFF;

        // Atomically switch: SEL1 off, SEL2 on
        uint32_t mask = (1u << SEL1_OUT) | (1u << SEL2_OUT);
        gpio_put_masked(mask, (1u << SEL1_OUT));  // SEL1=1, SEL2=0
        busy_wait_us_32(2);     // 2us settle
        uint8_t ah = ((gpio_get_all() & bus_mask) >> BUS_GPIO_START) & 0xFF;

        gpio_put(SEL2_OUT, 1);  // muxes off
        uint16_t addr = al | ((uint16_t)ah << 8);

        if (!gpio_get(WR_INPUT)) {
            gpio_put(SEL3_OUT, 0); gpio_put(DIR3_OUT, 0);
            busy_wait_us_32(3);
            uint8_t data = ((gpio_get_all() & bus_mask) >> BUS_GPIO_START) & 0xFF;
            mmu_write(addr, data);
            mem_w_op = data; m_adr = addr; d_adr = addr; dw_op++;
            while (!gpio_get(MREQ_INPUT)) tight_loop_contents();
            gpio_put(SEL3_OUT, 1);
        } else if (!gpio_get(RD_INPUT)) {
            uint8_t data = mmu_read(addr);
            mem_r_op = data; m_adr = addr; d_adr = addr; dr_op++;
            gpio_put(SEL3_OUT, 0); gpio_put(DIR3_OUT, 1);
            busy_wait_us_32(3);
            set_bus_dir(1);
            gpio_put_masked(bus_mask, (uint32_t)data << BUS_GPIO_START);
            while (!gpio_get(MREQ_INPUT)) tight_loop_contents();
            gpio_set_dir_masked(bus_mask, 0);
            gpio_put(SEL3_OUT, 1);
        }
        bus_active = false;
    }
}

// ===========================================================================
// SD card
// ===========================================================================

static FRESULT fr;
static FATFS   fs;
static FIL     fil;
static char    cwdbuf[FF_LFN_BUF] = {0};

char *init_and_mount_sd_card(void) {
    char fr_buf[10];
    memset(&cwdbuf, 0, FF_LFN_BUF);
    if (!spi_configured) show_error_and_halt("SD INIT ERR1");
    fr = f_mount(&fs, "", 1);
    if (fr != FR_OK) { sprintf(fr_buf, "SD INIT ERR2 %d", fr); show_error_and_halt(fr_buf); }
    fr = f_getcwd(cwdbuf, sizeof cwdbuf);
    if (FR_OK != fr) show_error_and_halt("SD INIT ERR3");
    return cwdbuf;
}

// ===========================================================================
// Z80NEO.INI reader
// ===========================================================================

int sd_read_init(void) {
    FRESULT fr_local;
    FATFS   fs_local;
    FIL     fil_local;
    int ret;
    TCHAR filename[] = "Z80NEO.INI";
    char buf[FILE_BUFF_SIZE];
    bool skip = false;

    clear_screen();
    if (!spi_configured) { print_string(3, 0, "SPI ERROR"); uart_puts(UART_ID, "SPI ERROR\r\n"); sleep_ms(DISPLAY_DELAY_LONG); skip = true; }

    char tmp_buf[10] = "";
    if (!skip) { fr_local = f_mount(&fs_local, "0", 0);
        if (FR_OK != fr_local) { print_string(3, 0, "MOUNT - ERROR"); uart_puts(UART_ID, "Mount error\r\n"); sleep_ms(DISPLAY_DELAY_LONG); skip = true; }
        else uart_puts(UART_ID, "Mount ok\r\n");
    }

    if (!skip) { fr_local = f_open(&fil_local, filename, FA_READ);
        if (fr_local != FR_OK) { print_string(0, 0, "OPEN - ERROR"); uart_puts(UART_ID, "Open error\r\n"); skip = true; while (true); }
        else uart_puts(UART_ID, "Open ok\r\n");
    }

    while (!skip) {
        if (!f_gets(MACHINE, sizeof(MACHINE), &fil_local)) { show_error(0, 0, "INI - MACHINE"); skip = true; break; }
        print_line(0, MACHINE); sleep_ms(DISPLAY_DELAY_SHORT);

        if (!f_gets(buf, sizeof(buf), &fil_local)) { show_error(0, 0, "INI - CANCEL2"); skip = true; break; }
        CANCEL2_ADC = decode_hex(buf[0]) * 16 * 16 + decode_hex(buf[1]) * 16 + decode_hex(buf[2]);
        print_line(0, "%CANCEL2: %03x     ", CANCEL2_ADC); sleep_ms(DISPLAY_DELAY_SHORT);

        if (!f_gets(buf, sizeof(buf), &fil_local)) { show_error(0, 0, "INI - CANCEL"); skip = true; break; }
        CANCEL_ADC = decode_hex(buf[0]) * 16 * 16 + decode_hex(buf[1]) * 16 + decode_hex(buf[2]);
        print_line(0, "%CANCEL : %03x     ", CANCEL_ADC); sleep_ms(DISPLAY_DELAY_SHORT);

        if (!f_gets(buf, sizeof(buf), &fil_local)) { show_error(0, 0, "INI - OK"); skip = true; break; }
        OK_ADC = decode_hex(buf[0]) * 16 * 16 + decode_hex(buf[1]) * 16 + decode_hex(buf[2]);
        print_line(0, "%OK     : %03x     ", OK_ADC); sleep_ms(DISPLAY_DELAY_SHORT);

        if (!f_gets(buf, sizeof(buf), &fil_local)) { show_error(0, 0, "INI - BACK"); skip = true; break; }
        BACK_ADC = decode_hex(buf[0]) * 16 * 16 + decode_hex(buf[1]) * 16 + decode_hex(buf[2]);
        print_line(0, "%BACK   : %03x     ", BACK_ADC); sleep_ms(DISPLAY_DELAY_SHORT);

        if (!f_gets(buf, sizeof(buf), &fil_local)) { show_error(0, 0, "INI - DOWN"); skip = true; break; }
        DOWN_ADC = decode_hex(buf[0]) * 16 * 16 + decode_hex(buf[1]) * 16 + decode_hex(buf[2]);
        print_line(0, "%DOWN   : %03x     ", DOWN_ADC); sleep_ms(DISPLAY_DELAY_SHORT);

        if (!f_gets(buf, sizeof(buf), &fil_local)) { show_error(0, 0, "INI - UP"); skip = true; break; }
        UP_ADC = decode_hex(buf[0]) * 16 * 16 + decode_hex(buf[1]) * 16 + decode_hex(buf[2]);
        print_line(0, "%UP     : %03x     ", UP_ADC); sleep_ms(DISPLAY_DELAY_SHORT);

        if (!f_gets(BANK_PROG[0], sizeof(BANK_PROG[0]), &fil_local)) { show_error(0, 0, "INI - PROG1"); skip = true; break; }
        print_line(0, "P1: %12s", BANK_PROG[0]); sleep_ms(DISPLAY_DELAY_SHORT);
        if (!f_gets(BANK_PROG[1], sizeof(BANK_PROG[1]), &fil_local)) { show_error(0, 0, "INI - PROG2"); skip = true; break; }
        print_line(0, "P2: %12s", BANK_PROG[1]); sleep_ms(DISPLAY_DELAY_SHORT);
        if (!f_gets(BANK_PROG[2], sizeof(BANK_PROG[2]), &fil_local)) { show_error(0, 0, "INI - PROG3"); skip = true; break; }
        print_line(0, "P3: %12s", BANK_PROG[2]); sleep_ms(DISPLAY_DELAY_SHORT);
        if (!f_gets(BANK_PROG[3], sizeof(BANK_PROG[3]), &fil_local)) { show_error(0, 0, "INI - PROG4"); skip = true; break; }
        print_line(0, "P4: %12s", BANK_PROG[3]); sleep_ms(DISPLAY_DELAY_SHORT);
        break;
    }

    if (!skip && f_gets(buf, sizeof(buf), &fil_local)) {
        DEBUG_ADC = buf[0] == '1';
        print_line(0, "%ADC DEBUG: %01x    ", DEBUG_ADC);
        sleep_ms(DISPLAY_DELAY_SHORT); clear_screen();
    }
    fr_local = f_close(&fil_local);
    if (fr_local != FR_OK) { show_error(0, 0, "INI - CLOSE"); while (true); }
    f_unmount("0:");
    return 0;
}

// ===========================================================================
// Intel HEX loader (same as before, with bank offset masking)
// ===========================================================================

void load_file(bool quiet) {
    reset_hold();
    FRESULT fr_local;
    FATFS   fs_local;
    FIL     fil_local;
    char buf[FILE_BUFF_SIZE];
    char const *p_dir;

    clear_bank(cur_bank);

    if (!quiet) { clear_screen(); print_string(0, 0, "Loading HEX"); print_string(0, 1, file); sleep_ms(DISPLAY_DELAY_SHORT); }

    p_dir = init_and_mount_sd_card();
    fr_local = f_open(&fil_local, file, FA_READ);
    if (fr_local != FR_OK) { sleep_ms(DISPLAY_DELAY_LONG); show_error(0, 0, "Can't open file!"); sleep_ms(DISPLAY_DELAY_LONG); show_error(0, 0, file); sleep_ms(DISPLAY_DELAY_LONG); return; }

    uint8_t  intel_checksum         = 0;
    uint8_t  intel_byte_count       = 0;
    uint16_t intel_address          = 0;
    uint8_t  intel_record_type      = 0;
    uint32_t intel_extended_address = 0;
    uint32_t intel_absolute_address = 0;

    uint8_t parser_state = 0;
    uint8_t hex_digit     = 0;
    uint8_t current_byte  = 0;
    uint8_t data_index    = 0;
    uint8_t data_buffer[256];  // max 255 bytes per Intel HEX record

    int      line         = 0;
    uint32_t bytes_loaded = 0;

    if (DEBUG_LOAD) { clear_screen(); print_string(center_string("--HEX LOADER--"), 0, "--HEX LOADER--"); print_string(0, 1, file); print_string(0, 2, "ADDR:          "); print_string(0, 3, "BANK:          "); print_string(0, 4, "DATA:          "); print_string(0, 5, "LINE:          "); }
    uart_puts(UART_ID, "Loading hex\r\n");

    while (true) {
        sprintf(serial_text_buffer, "%c\r", load_chars[load_char_index]); uart_puts(UART_ID, serial_text_buffer);
        load_char_index += 1; if (load_char_index > strlen(load_chars)) load_char_index = 0;
        memset(&buf, 0, sizeof(buf));
        if (!f_gets(buf, sizeof(buf), &fil_local)) break;
        int i = 0;
        if (buf[0] == '\0' || buf[0] == '\r' || buf[0] == '\n') continue;
        if (buf[0] != ':') { sprintf(text_buffer, "Not HEX Line: %d Data: %016x", line, buf); uart_puts(UART_ID, text_buffer); clear_screen(); fr_local = f_close(&fil_local); return; }

        parser_state = 1; intel_byte_count = 0; intel_address = 0; intel_record_type = 0; intel_checksum = 0; hex_digit = 0; current_byte = 0; data_index = 0;
        cur_bank = 0; i = 1;

        while (true) {
            byte b = buf[i++];
            if (!b || b == '\n' || b == '\r') break;
            if (b == ' ' || b == '\t') continue;
            int decoded = decode_hex(b);
            if (decoded == -1) { sprintf(text_buffer, "Invalid HEX: Line %05d", line); uart_puts(UART_ID, text_buffer); clear_screen(); fr_local = f_close(&fil_local); return; }

            if (hex_digit == 0) { current_byte = decoded * 16; hex_digit = 1; }
            else { current_byte += decoded; hex_digit = 0;
                if (parser_state < 6) intel_checksum += current_byte;
                switch (parser_state) {
                case 1: intel_byte_count = current_byte; parser_state = 2; break;
                case 2: cur_bank = (current_byte & 0b11000000) >> 6; intel_address = current_byte << 8; parser_state = 3; break;
                case 3: intel_address |= current_byte; parser_state = 4; break;
                case 4: intel_record_type = current_byte; parser_state = (intel_byte_count == 0) ? 6 : (data_index = 0, 5); break;
                case 5: data_buffer[data_index++] = current_byte; if (data_index >= intel_byte_count) parser_state = 6; break;
                case 6:
                    if ((intel_checksum + current_byte) & 0xFF) { sprintf(text_buffer, "Checksum err Line %05d", line); uart_puts(UART_ID, text_buffer); clear_screen(); fr_local = f_close(&fil_local); return; }
                    switch (intel_record_type) {
                    case 0: intel_absolute_address = intel_extended_address + intel_address;
                        { uint16_t bank_offset = intel_absolute_address & 0x3FFF;
                          for (int j = 0; j < intel_byte_count; j++) {
                              uint8_t val = data_buffer[j];
                              uint32_t abs_addr = intel_absolute_address + j;
                              if (abs_addr < 0x4000) {
                                  // Bootloader code: replicate to ALL banks
                                  for (int bk = 0; bk < MAX_BANKS; bk++)
                                      ram[bk][bank_offset + j] = val;
                              } else {
                                  // OS/data: put each 16KB page in its own bank
                                  uint8_t page = (abs_addr >> 14) & 0x03;
                                  ram[page][bank_offset + j] = val;
                              }
                              bytes_loaded++;
                          }
                        }
                        break;
                    case 1: break;
                    case 2: if (intel_byte_count == 2) intel_extended_address = (data_buffer[0] << 8 | data_buffer[1]) * 16; break;
                    case 3: case 5: break;
                    case 4: if (intel_byte_count == 2) intel_extended_address = (data_buffer[0] << 8 | data_buffer[1]) << 16; break;
                    default: sprintf(text_buffer, "Unknown type %02X", intel_record_type); print_string(0, 5, text_buffer); break;
                    }
                    parser_state = 0; break;
                }
            }
        }
        line++;
        if (intel_record_type == 1) break;
    }
    fr_local = f_close(&fil_local); if (fr_local != FR_OK) show_error(0, 0, "Can't close file!");
    f_unmount("0:");
    if (DEBUG_LOAD) { sprintf(text_buffer, "Loaded %lu bytes", bytes_loaded); uart_puts(UART_ID, text_buffer); uart_puts(UART_ID, "\r\n"); print_string(0, 6, text_buffer); sleep_ms(DISPLAY_DELAY_LONG); }
    if (!quiet) { clear_screen(); sprintf(text_buffer, "Loaded: %lu bytes", bytes_loaded); print_string(0, 0, text_buffer); print_string(0, 1, file); sleep_ms(DISPLAY_DELAY); }
    strcpy(BANK_PROG[cur_bank], file);
    reset_release();
}

void load(void) { select_file(); load_file(false); }

void load_init_progs(void) {
    // Load only the first configured program (the hex file contains everything)
    if (BANK_PROG[0][0] >= 48) {
        strcpy(file, BANK_PROG[0]);
        load_file(true);
    }
    cur_bank = 0;
    // Lock RST vectors after loading — hex loader wrote correct bytes,
    // now prevent Z80 from corrupting them during execution.
    rst_locked = true;
}

// ===========================================================================
// Initialization — called from main.c
// ===========================================================================

void bus_callback(uint pin, uint32_t events) { /* GPIO-based; unused */ }
void save(void) {}
void load_hex_from_uart(void) {}
void pio_bus_init(void) { /* PWM ISR mode; unused */ }
