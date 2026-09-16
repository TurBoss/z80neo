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
#include "i2c_ee.h"
#include "cpm_disk.h"

// ===========================================================================
// Globals
// ===========================================================================

uint8_t ram[MAX_BANKS][(uint16_t)RAM_SIZE] = {};

uint8_t *psram_base = NULL;

volatile uint8_t cur_bank = 0;

volatile uint16_t m_adr = 0x0000;

volatile uint8_t io_r_op  = 0x00;
volatile uint8_t io_w_op  = 0x00;
volatile uint8_t mem_r_op = 0x00;
volatile uint8_t mem_w_op = 0x00;

char    rx_buffer[UART_BUF_SIZE];
uint16_t rx_count         = 0;
uint16_t rx_index         = 0;
uint16_t rx_read          = 0;
bool    rx_data_available = false;

volatile uint8_t serial_status_1 = 0x02;
uint8_t  tx_buffer[UART_TX_BUF_SIZE];
uint16_t tx_head = 0;
uint16_t tx_tail = 0;
uint16_t tx_count = 0;

// Terminal local-echo cancellation.  If the terminal echoes what the Pico
// sends, that echo arrives on RX and the Z80 reads its own output back as
// input (which makes the CCP see spurious commands / Ctrl-C -> warm-boot loop).
// Track recently transmitted bytes together with their send time; an RX byte
// that matches the oldest un-echoed byte *within the round-trip window* is the
// terminal's echo and is dropped.  The time window is what keeps a terminal
// with local echo OFF from losing genuine keystrokes that happen to repeat a
// character the Z80 just printed (that match arrives far later than an echo).
#define ECHO_FIFO_SIZE 64
#define ECHO_WINDOW_US 30000   // 30 ms — longer than a byte's round trip

static uint8_t  echo_fifo[ECHO_FIFO_SIZE];
static uint32_t echo_time[ECHO_FIFO_SIZE];
static uint16_t echo_head = 0, echo_tail = 0, echo_count = 0;

void echo_track_tx(uint8_t c) {
    if (echo_count >= ECHO_FIFO_SIZE) {
        // More output than can still be in flight as echo — evict the oldest
        // so the FIFO always holds the most recent transmissions.
        echo_tail = (echo_tail + 1) % ECHO_FIFO_SIZE;
        echo_count--;
    }
    echo_fifo[echo_head] = c;
    echo_time[echo_head] = time_us_32();
    echo_head = (echo_head + 1) % ECHO_FIFO_SIZE;
    echo_count++;
}

static bool echo_filter_rx(uint8_t c) {
    if (echo_count == 0) return false;
    if (echo_fifo[echo_tail] != c) return false;
    if ((uint32_t)(time_us_32() - echo_time[echo_tail]) > ECHO_WINDOW_US)
        return false;   // stale match — real input, not an echo
    echo_tail = (echo_tail + 1) % ECHO_FIFO_SIZE;
    echo_count--;
    return true;        // drop: terminal echoed our own output
}

uint32_t d_adr = 0;
uint32_t dr_op = 0;
uint32_t dw_op = 0;
uint32_t io_op = 0;

uint32_t bus_mask = 0;

volatile bool disabled  = false;
volatile bool system_up = false;    // set by main() after boot — gates display-loop Z80 control

volatile uint16_t CANCEL2_ADC = 0xFFF;
volatile uint16_t CANCEL_ADC  = 0xBFF;
volatile uint16_t OK_ADC      = 0x7FF;
volatile uint16_t BACK_ADC    = 0x5FF;
volatile uint16_t DOWN_ADC    = 0x2FF;
volatile uint16_t UP_ADC      = 0x0FF;

volatile bool DEBUG_ADC = false;

// Console (port 0x80) traffic ring: bit7 set = Z80 write, clear = Z80 read.
volatile uint8_t  diag_con[64];
volatile uint32_t diag_con_idx = 0;

bool spi_configured;

char    MACHINE[FILE_LENGTH] = "Z80 CPU";
char    BANK_PROG[4][FILE_LENGTH];

volatile uint32_t last_bytes_loaded = 0;

// ===========================================================================
// Helpers
// ===========================================================================

int decode_hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

// ===========================================================================
// Bank management
// ===========================================================================

void clear_bank(uint8_t bank) {
    if (bank < MAX_BANKS) {
        for (uint16_t adr = 0; adr < RAM_SIZE; adr++)
            ram[bank][adr] = 0;
        if (bank < 4) memset(BANK_PROG[bank], 0, FILE_LENGTH);
    } else if (psram_base) {
        volatile uint8_t *p = psram_base + (bank - MAX_BANKS) * RAM_SIZE;
        for (uint16_t adr = 0; adr < RAM_SIZE; adr++)
            p[adr] = 0;
    }
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

uint8_t mmu_read(uint16_t addr) {
    uint8_t page = (addr >> 14) & 0x03;
    return ram[phys_to_bank(mmu_page[page])][addr & 0x3FFF];
}

// After load_init_progs, lock RST vectors against accidental Z80 writes
static volatile bool rst_locked = false;

// Diagnostics: track writes that land in the BIOS code area (0xEB00-0xEE00)
volatile uint16_t diag_bios_wr_addr = 0;
volatile uint8_t  diag_bios_wr_data = 0;
volatile uint32_t diag_bios_wr_count = 0;

// Diagnostics: track the value the hex loader stores at 0xEBEE
volatile uint8_t  diag_hex_ebee = 0;
volatile uint32_t diag_hex_ebee_count = 0;
volatile char     diag_hex_raw[64] = {0};

void mmu_write(uint16_t addr, uint8_t data) {
    // z80neo: write-protect only the JP opcode byte at 0x0000 in bank 0.  The
    // BIOS installs the warm-boot vector as LD (0001H),HL (bytes 0x0001/0x0002
    // = WBOOT), so those must stay writable or every warm boot falls back to
    // the loader's cold-boot JP and reprints the signon.  rst_locked protects
    // against errant Z80 code corrupting the restart opcode itself.
    if (rst_locked && addr == 0 && mmu_page[0] == 0x20) return;
    if (addr >= 0xEB00 && addr <= 0xEE00) {
        diag_bios_wr_addr = addr;
        diag_bios_wr_data = data;
        diag_bios_wr_count++;
    }
    uint8_t page = (addr >> 14) & 0x03;
    ram[phys_to_bank(mmu_page[page])][addr & 0x3FFF] = data;
}

// ===========================================================================
// I/O device handler — UART + MMU ports (handled by GPIO IRQ on IORQ)
// ===========================================================================

uint8_t io_read_port(uint8_t port) {
    switch (port) {
    case SERIAL_PORT_1:  // 0x80 — serial data (repo build)
    case 0x83: {         // 0x83 — serial data (older SD-card build)
        // Pick up anything the main loop has not drained yet.  read_uart_char()
        // returns 0 only when the ring is empty; with the bus settle fixed the
        // Z80 no longer samples this port ahead of CONSTAT, so a spurious NUL
        // (which CP/M would echo as '^@') does not occur.
        uart_rx_poll();
        uint8_t ch = read_uart_char();
        rx_data_available = (rx_count > 0);
        serial_status_1 = rx_data_available ? (serial_status_1 | 0x01) : (serial_status_1 & ~0x01);
        diag_con[diag_con_idx++ & 63] = ch & 0x7F;
        return ch;
    }
    case 0xD1: return i2c_ee_read();
    case 0xE0: return cpm_disk_read_port(0xE0);
    case 0xE2: return cpm_disk_read_port(0xE2);
    case SERIAL_STATUS_1:
        // Drain the HW FIFO first so a byte already received is reflected, then
        // derive the ready bit from the ring itself.  rx_data_available can go
        // stale (set without a ring byte), and a stale "ready" makes CONSTAT
        // lie: the Z80 then reads a phantom byte, and CP/M echoes it as '^@'.
        uart_rx_poll();
        rx_data_available = (rx_count > 0);
        serial_status_1 = rx_data_available ? (serial_status_1 | 0x01) : (serial_status_1 & ~0x01);
        return serial_status_1;
    case 0xF0: return mmu_page[0];
    case 0xF1: return mmu_page[1];
    case 0xF2: return mmu_page[2];
    case 0xF3: return mmu_page[3];
    default: return 0x00;
    }
}

void io_write_port(uint8_t port, uint8_t data) {
    switch (port) {
    case SERIAL_PORT_1:  // 0x80 — serial data (repo build)
    case 0x83:           // 0x83 — serial data (older SD-card build)
        diag_con[diag_con_idx++ & 63] = data | 0x80;
        if (tx_count < UART_TX_BUF_SIZE) {
            tx_buffer[tx_head++] = data;
            if (tx_head >= UART_TX_BUF_SIZE) tx_head = 0;
            tx_count++;
        }
        serial_status_1 = (tx_count < UART_TX_BUF_SIZE) ? (serial_status_1 | 0x02) : (serial_status_1 & ~0x02);
        break;
    case 0xF0: mmu_page[0] = data; break;
    case 0xF1: mmu_page[1] = data; break;
    case 0xF2: mmu_page[2] = data; break;
    case 0xF3: mmu_page[3] = data; break;
    case 0xD1: i2c_ee_write(data); return;
    case 0xE1: cpm_disk_write_port(0xE1, data); return;
    case 0xE2: cpm_disk_write_port(0xE2, data); return;
    case 0xE0: cpm_disk_write_port(0xE0, data); return;
    case 0xE3: cpm_disk_write_port(0xE3, data); return;  // drive select
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
}

// ===========================================================================
// Bus control (used by IO handler only — PIO handles memory cycles)
// ===========================================================================

void set_bus_dir(int direction) {
    if (direction) gpio_set_dir_masked(bus_mask, bus_mask);
    else           gpio_set_dir_masked(bus_mask, 0);
}

// ===========================================================================
// UART
// ===========================================================================

// Drain the hardware UART RX FIFO into the software ring.  Single place that
// touches rx_index/rx_read/rx_count so the main loop and the I/O port handler
// cannot disagree about the buffer state.  If the ring is full the oldest byte
// is dropped (console type-ahead semantics).
void uart_rx_poll(void) {
    while (uart_is_readable(UART_ID)) {
        uint8_t c = uart_getc(UART_ID);
        if (echo_filter_rx(c)) continue;  // terminal local echo — drop it
        if (rx_count >= UART_BUF_SIZE) {
            rx_read = (rx_read + 1) % UART_BUF_SIZE;
            rx_count--;
        }
        rx_buffer[rx_index] = c;
        rx_index = (rx_index + 1) % UART_BUF_SIZE;
        rx_count++;
        rx_data_available = true;
    }
}

char read_uart_char(void) {
    // Only return from software buffer — main loop drains UART FIFO
    if (rx_count > 0) {
        char ch = rx_buffer[rx_read];
        rx_read = (rx_read + 1) % UART_BUF_SIZE;
        rx_count--;
        if (!rx_count) rx_data_available = false;
        return ch;
    }
    rx_data_available = false;
    return 0;
}

void uart_status_handler(void) {
    while (tx_count > 0 && uart_is_writable(UART_ID)) {
        uart_putc(UART_ID, tx_buffer[tx_tail]);
        tx_tail = (tx_tail + 1) % UART_TX_BUF_SIZE;
        tx_count--;
    }
    uart_rx_poll();
    rx_data_available = (rx_count > 0);
    serial_status_1 = (uart_is_writable(UART_ID) && tx_count < UART_TX_BUF_SIZE)
                      ? (serial_status_1 | 0x02) : (serial_status_1 & ~0x02);
    if (rx_data_available) serial_status_1 |= 0x01;
    else                   serial_status_1 &= ~0x01;
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
        if (buf[0] != ':') { sprintf(text_buffer, "Not HEX Line: %d Data: %s", line, buf); uart_puts(UART_ID, text_buffer); clear_screen(); fr_local = f_close(&fil_local); return; }

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
                    if (intel_address == 0xEBE0) { strncpy((char*)diag_hex_raw, buf, sizeof(diag_hex_raw)-1); }
                    if ((intel_checksum + current_byte) & 0xFF) { sprintf(text_buffer, "Checksum err Line %05d", line); uart_puts(UART_ID, text_buffer); clear_screen(); fr_local = f_close(&fil_local); return; }
                    switch (intel_record_type) {
                    case 0: intel_absolute_address = intel_extended_address + intel_address;
                        { uint16_t bank_offset = intel_absolute_address & 0x3FFF;
                          for (int j = 0; j < intel_byte_count; j++) {
                              uint8_t val = data_buffer[j];
                              uint32_t abs_addr = intel_absolute_address + j;
                              if (abs_addr == 0xEBEE) { diag_hex_ebee = val; diag_hex_ebee_count++; }
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
    last_bytes_loaded = bytes_loaded;
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

// Boot-time sanity dump: shows the loaded program and the key page-zero /
// CCP-BDOS / BIOS vectors actually present in the banks.
void post_dump(void) {
    char b[160];
    sprintf(b,
        "POST: pgm=%s bytes=%lu\r\n"
        "  0000: %02X %02X %02X %02X\r\n"
        "  D400: %02X %02X %02X %02X\r\n"
        "  EB00: %02X %02X %02X %02X\r\n",
        BANK_PROG[0], (unsigned long)last_bytes_loaded,
        ram[0][0x0000], ram[0][0x0001], ram[0][0x0002], ram[0][0x0003],
        ram[3][0x1400], ram[3][0x1401], ram[3][0x1402], ram[3][0x1403],
        ram[3][0x2B00], ram[3][0x2B01], ram[3][0x2B02], ram[3][0x2B03]);
    uart_puts(UART_ID, b);
}
