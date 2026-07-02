// memory.c
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

#include "ff.h"
#include "tf_card.h"

#include "display.h"
#include "memory.h"
#include "utils.h"

// ===========================================================================
// Globals
// ===========================================================================

// RAM banks
uint8_t ram[MAX_BANKS][(uint16_t)RAM_SIZE] = {};
uint8_t mmuram[(uint16_t)RAM_SIZE]        = {};
uint8_t sdram[(uint16_t)SD_RAM_SIZE]      = {};

// Current bank
volatile uint8_t cur_bank = 0;

// CPU state
uint16_t pc     = 0;
uint8_t  opcode = 0;

volatile uint8_t t_clock = 0;
volatile uint8_t m_clock = 0;

uint8_t next_oclock = 0;
uint8_t next_mclock = 0;
uint8_t next_dclock = 0;

// Address
volatile uint8_t  low_adr  = 0x00;
volatile uint8_t  high_adr = 0x00;
volatile uint16_t m_adr    = 0x0000;

// I/O ops
volatile uint8_t io_r_op     = 0x00;
volatile uint8_t io_w_op     = 0x00;
volatile uint8_t opcode_r_op = 0x00;
volatile uint8_t mem_r_op    = 0x00;
volatile uint8_t mem_w_op    = 0x00;

// UART
char    rx_buffer[UART_BUF_SIZE];
uint8_t rx_count          = 0;
uint8_t rx_index          = 0;
uint8_t rx_read           = 0;
bool    rx_data_available = false;

volatile uint8_t serial_status_1 = 0x00;
volatile uint8_t serial_status_2 = 0x00;

// UART TX buffer (for non-blocking send from IRQ context)
#define UART_TX_BUF_SIZE 64
uint8_t  tx_buffer[UART_TX_BUF_SIZE];
uint8_t  tx_head = 0;
uint8_t  tx_tail = 0;
uint8_t  tx_count = 0;

// Debug counters
uint32_t d_adr = 0;
uint32_t dr_op = 0;
uint32_t dw_op = 0;

// Bus control signals
bool mreq      = true;
bool iorq      = true;
bool clk_level = true;
bool rd        = true;
bool wr        = true;

bool mreq_status = false;
bool iorq_status = false;

uint32_t bus_mask = 0;

// Shared state with display loop
volatile bool disabled  = false;
volatile bool read      = false;
volatile bool written   = false;

// ADC thresholds
volatile uint16_t CANCEL2_ADC = 0xFFF;
volatile uint16_t CANCEL_ADC  = 0xBFF;
volatile uint16_t OK_ADC      = 0x7FF;
volatile uint16_t BACK_ADC    = 0x5FF;
volatile uint16_t DOWN_ADC    = 0x2FF;
volatile uint16_t UP_ADC      = 0x0FF;

volatile bool DEBUG_ADC = false;

// SPI
bool spi_configured;

// PWM
uint slice;

// Misc
char    MACHINE[FILE_LENGTH] = "Z80 CPU";
char    BANK_PROG[4][FILE_LENGTH];
uint8_t in_bytes[1024];
char    log_buf[32];

// ===========================================================================
// Helpers
// ===========================================================================

unsigned char decode_hex(char c) {
    if (c >= 65 && c <= 70)
        return c - 65 + 10;
    else if (c >= 97 && c <= 102)
        return c - 97 + 10;
    else if (c >= 48 && c <= 67)
        return c - 48;
    else
        return -1;
}

// ===========================================================================
// Bank management
// ===========================================================================

void clear_bank(uint8_t bank) {
    for (uint16_t adr = 0; adr < RAM_SIZE; adr++) {
        ram[bank][adr] = 0;
    }
    memset(BANK_PROG[bank], 0, FILE_LENGTH);
}

// ===========================================================================
// PWM IRQ bus-cycle state machine constants
// ===========================================================================

enum { CYC_IDLE = 0, CYC_ADDR_READ, CYC_DATA_XFER };

// ===========================================================================
// MMU — 4 pages of 16 KB, each mapping to one of 4 banks
// Ports: 0xF0 = page 0 (0x0000–0x3FFF), …, 0xF3 = page 3 (0xC000–0xFFFF)
// ===========================================================================

static uint8_t mmu_page[4] = {0, 0, 0, 0};  // default: all pages → bank 0

static inline uint8_t mmu_read(uint16_t addr) {
    uint8_t page = (addr >> 14) & 0x03;
    return ram[mmu_page[page]][addr & 0x3FFF];
}

static inline void mmu_write(uint16_t addr, uint8_t data) {
    uint8_t page = (addr >> 14) & 0x03;
    ram[mmu_page[page]][addr & 0x3FFF] = data;
}

// ===========================================================================
// I/O device handler — UART at ports 0x80 (data) and 0x81 (status)
// ===========================================================================

static uint8_t io_state = 0;  // mirrors CYC_IDLE / CYC_ADDR_READ / CYC_DATA_XFER
static uint8_t io_port  = 0;

static uint8_t io_read_port(uint8_t port) {
    switch (port) {
    case SERIAL_PORT_1:   // 0x80 — data
        return read_uart_char();
    case SERIAL_STATUS_1: // 0x81 — status
        return serial_status_1;
    case MMU_PAGE_0: case MMU_PAGE_1: case MMU_PAGE_2: case MMU_PAGE_3:
        return mmu_page[port - MMU_PAGE_0];
    default:
        return 0xFF;
    }
}

static void io_write_port(uint8_t port, uint8_t data) {
    switch (port) {
    case SERIAL_PORT_1:   // 0x80 — data (buffered, non-blocking from IRQ)
        if (tx_count < UART_TX_BUF_SIZE) {
            tx_buffer[tx_head] = data;
            tx_head = (tx_head + 1) % UART_TX_BUF_SIZE;
            tx_count++;
        }
        break;
    case MMU_PAGE_0: case MMU_PAGE_1: case MMU_PAGE_2: case MMU_PAGE_3:
        mmu_page[port - MMU_PAGE_0] = data % MAX_BANKS;
        break;
    case SERIAL_STATUS_1: // 0x81 — status (write resets? ignore for now)
    default:
        break;
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

    t_clock = CYC_IDLE;
    m_clock = 0;
}

// ===========================================================================
// Bus control
// ===========================================================================

// 0 IN, 1 OUT
void set_bus_dir(int direction) {
    if (direction) {
        gpio_set_dir_masked(bus_mask, bus_mask);
    } else {
        gpio_set_dir_masked(bus_mask, 0);
    }
}

void nop_delay(void) {
    // 125 MHz — 3 nops ≈ 20 ns
    asm volatile(" nop\n nop\n nop\n");
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

// Helper: advance a circular-buffer index
static inline uint8_t circ_next(uint8_t idx) {
    return (idx + 1) % UART_BUF_SIZE;
}

char read_uart_char(void) {
    if (rx_count > 0) {
        char ch = rx_buffer[rx_read];
        rx_read = circ_next(rx_read);
        rx_count--;
        return ch;
    } else {
        rx_data_available = false;
        return '\0';
    }
}

void uart_status_handler(void) {
    // Flush buffered TX bytes to hardware UART
    while (tx_count > 0 && uart_is_writable(UART_ID)) {
        uart_putc_raw(UART_ID, tx_buffer[tx_tail]);
        tx_tail = (tx_tail + 1) % UART_TX_BUF_SIZE;
        tx_count--;
    }

    // TX ready = hardware writable AND buffer not full (so Z80 can queue more)
    if (uart_is_writable(UART_ID) && tx_count < UART_TX_BUF_SIZE) {
        serial_status_1 |= 0x02;   // set bit 1 (TX ready)
    } else {
        serial_status_1 &= ~0x02;  // clear bit 1
    }

    if (rx_data_available) {
        serial_status_1 |= 0x01;   // set bit 0 (RX data available)
    } else {
        serial_status_1 &= ~0x01;  // clear bit 0
    }
}

// ===========================================================================
// GPIO bus callback (edge-triggered, currently unused in main())
// ===========================================================================

void bus_callback(uint pin, uint32_t events) {
    if (pin == IORQ_INPUT) {
        if (events == 0x4) {
            iorq = true;
        } else if (events == 0x8) {
            iorq = false;
        }
    } else if (pin == MREQ_INPUT) {
        if (events == 0x4) {
            mreq = true;
        } else if (events == 0x8) {
            mreq = false;
        }
    }
}

// ===========================================================================
// Bus multiplexer control
// ===========================================================================

void bus_manager(int value) {
    /*
     * 0 = OFF
     * 1 = ADDR LOW
     * 2 = ADDR HIGH
     * 3 = DATA
     */

    if (value == 0) {  // OFF
        gpio_put(DIR1_OUT, 1); gpio_put(DIR2_OUT, 1); gpio_put(DIR3_OUT, 1);
        gpio_put(SEL1_OUT, 1); gpio_put(SEL2_OUT, 1); gpio_put(SEL3_OUT, 1);
    } else if (value == 1) {  // ADDRESS LOW
        gpio_put(DIR1_OUT, 0); gpio_put(DIR2_OUT, 1); gpio_put(DIR3_OUT, 1);
        gpio_put(SEL1_OUT, 0); gpio_put(SEL2_OUT, 1); gpio_put(SEL3_OUT, 1);
    } else if (value == 2) {  // ADDRESS HIGH
        gpio_put(DIR1_OUT, 1); gpio_put(DIR2_OUT, 0); gpio_put(DIR3_OUT, 1);
        gpio_put(SEL1_OUT, 1); gpio_put(SEL2_OUT, 0); gpio_put(SEL3_OUT, 1);
    } else if (value == 3) {  // DATA
        gpio_put(DIR1_OUT, 1); gpio_put(DIR2_OUT, 1); gpio_put(DIR3_OUT, 1);
        gpio_put(SEL1_OUT, 1); gpio_put(SEL2_OUT, 1); gpio_put(SEL3_OUT, 0);
    }
}

// ===========================================================================
// PWM IRQ — bus cycle state machine
//
// Fires on every rising edge of the Z80 clock (10 Hz → every 100 ms).
//
// States:
//   IDLE       — waiting for /MREQ low.  When seen: read address.
//   ADDR_READ  — address was read last IRQ.  Now determine cycle type from
//                /RD or /WR and perform the data transfer.
//   DATA_XFER  — data is on the bus (for reads) or was stored (for writes).
//                Wait until /MREQ goes high, then release the bus.
//
// M1 refresh (T4) is detected when ADDR_READ sees neither /RD nor /WR low.
// ===========================================================================

void pwm_irq_handler(void) {
    pwm_clear_irq(pwm_gpio_to_slice_num(GPIO_PWM_SIG));

    // ── Sample all bus control signals ──────────────────────────────
    bool rd_low   = !gpio_get(RD_INPUT);
    bool wr_low   = !gpio_get(WR_INPUT);
    bool mreq_low = !gpio_get(MREQ_INPUT);
    bool iorq_low = !gpio_get(IORQ_INPUT);

    // Update module-level variables for debug display
    rd   = !rd_low;
    wr   = !wr_low;
    mreq = !mreq_low;
    iorq = !iorq_low;

    if (DEBUG_IO) {
        sprintf(log_buf, "CLK st:%d -- RD:%d WR:%d MREQ:%d IORQ:%d\r\n",
                t_clock, rd_low, wr_low, mreq_low, iorq_low);
        uart_puts(UART_ID, log_buf);
    }

    // ── Bus idle — both /MREQ and /IORQ high ────────────────────────
    if (!mreq_low && !iorq_low) {
        // Only release the memory bus if we actually completed a transfer
        // (CYC_DATA_XFER).  CYC_ADDR_READ means we read the address but
        // never delivered data — the cycle was aborted (e.g. spurious
        // interrupt), so just reset without touching the GPIO bus.
        if (t_clock == CYC_DATA_XFER) {
            bus_manager(0);
            gpio_put_masked(bus_mask, 0);
            gpio_set_dir_masked(bus_mask, 0);
            if (DEBUG_IO) uart_puts(UART_ID, "\t\tBUS RELEASE (mem)\r\n");
        }
        if (io_state == 2) {
            bus_manager(0);
            gpio_put_masked(bus_mask, 0);
            gpio_set_dir_masked(bus_mask, 0);
            if (DEBUG_IO) uart_puts(UART_ID, "\t\tBUS RELEASE (io)\r\n");
        }
        t_clock  = CYC_IDLE;
        io_state = 0;
        return;
    }

    // ================================================================
    // I/O CYCLE  (/IORQ low)
    // ================================================================
    if (iorq_low) {
        switch (io_state) {

        case 0:  // T1: read port address from A0–A7
            bus_manager(1);  set_bus_dir(0);
            sleep_us(INST_DELAY);
            io_port = ((gpio_get_all() & bus_mask) >> BUS_GPIO_START) & 0xFF;
            bus_manager(0);
            gpio_set_dir_masked(bus_mask, 0);
            if (DEBUG_IO) {
                sprintf(log_buf, "\t\tI/O PORT 0x%02X\r\n", io_port);
                uart_puts(UART_ID, log_buf);
            }
            if (wr_low) {
                // OUT: Z80 puts data on D0–D7 at T1, read it now
                gpio_put(DIR1_OUT,1); gpio_put(DIR2_OUT,1); gpio_put(DIR3_OUT,0);
                gpio_put(SEL1_OUT,1); gpio_put(SEL2_OUT,1); gpio_put(SEL3_OUT,0);
                gpio_set_dir_masked(bus_mask, 0);
                sleep_us(INST_DELAY);
                io_w_op = (gpio_get_all() & bus_mask) >> BUS_GPIO_START;
                io_write_port(io_port, io_w_op);
                gpio_set_dir_masked(bus_mask, 0);
                bus_manager(0);
                if (DEBUG_IO) {
                    sprintf(log_buf, "\t\tOUT 0x%02X -> port 0x%02X\r\n",
                            io_w_op, io_port);
                    uart_puts(UART_ID, log_buf);
                }
                io_state = 2;  // done in one shot
            } else {
                io_state = 1;  // wait for T2 to see /RD
            }
            break;

        case 1:  // T2: /RD low → read from device, output to data bus
            if (rd_low) {
                io_r_op = io_read_port(io_port);
                bus_manager(3);  set_bus_dir(1);
                sleep_us(INST_DELAY);
                gpio_set_dir_masked(bus_mask, bus_mask);
                gpio_put_masked(bus_mask, (uint32_t)io_r_op << BUS_GPIO_START);
                sleep_us(INST_DELAY);
                if (DEBUG_IO) {
                    sprintf(log_buf, "\t\tIN   0x%02X <- port 0x%02X\r\n",
                            io_r_op, io_port);
                    uart_puts(UART_ID, log_buf);
                }
                io_state = 2;
            }
            break;

        case 2:  // T3: data on bus, wait for /IORQ↑ to release
            break;
        }
        return;
    }

    // ================================================================
    // MEMORY CYCLE  (/MREQ low)
    // ================================================================
    if (mreq_low) {

    switch (t_clock) {

    // ================================================================
    // CYC_IDLE — first sight of /MREQ low: read address.
    // If /RD or /WR is already low (we missed T1 due to M1 refresh
    // cleanup), do the data transfer immediately so the Z80 gets
    // valid data before the next T3 sampling edge.
    // ================================================================
    case CYC_IDLE:
        // Read low address (A0–A7) via 74LS245 #1
        bus_manager(1);
        set_bus_dir(0);
        sleep_us(INST_DELAY);
        low_adr = ((gpio_get_all() & bus_mask) >> BUS_GPIO_START) & 0xFF;

        // Read high address (A8–A15) via 74LS245 #2
        bus_manager(2);
        set_bus_dir(0);
        sleep_us(INST_DELAY);
        high_adr = ((gpio_get_all() & bus_mask) >> BUS_GPIO_START) & 0xFF;

        bus_manager(0);
        gpio_set_dir_masked(bus_mask, 0);
        m_adr = low_adr | (high_adr << 8);

        if (DEBUG_IO) {
            sprintf(log_buf, "\t\tADDR 0x%04X\r\n", m_adr);
            uart_puts(UART_ID, log_buf);
        }

        // If signals are already asserted, we're in T2 (missed T1).
        // Handle the data transfer now so it's on the bus before T3.
        if (rd_low) {
            // ── Memory READ: output data from RAM to Z80 data bus ──
            mem_r_op = mmu_read(m_adr);

            bus_manager(3);              // SEL3=0, DIR3=1 → Pico→Z80
            set_bus_dir(1);             // GPIO outputs
            sleep_us(INST_DELAY);
            gpio_set_dir_masked(bus_mask, bus_mask);
            gpio_put_masked(bus_mask, (uint32_t)mem_r_op << BUS_GPIO_START);
            sleep_us(INST_DELAY);

            if (DEBUG_IO) {
                sprintf(log_buf, "\t\tREAD  0x%02X -> CPU\r\n", mem_r_op);
                uart_puts(UART_ID, log_buf);
            }
            t_clock = CYC_DATA_XFER;

        } else if (wr_low) {
            // ── Memory WRITE: read data from Z80, store to RAM ─────
            gpio_put(DIR1_OUT, 1);  gpio_put(DIR2_OUT, 1);  gpio_put(DIR3_OUT, 0);
            gpio_put(SEL1_OUT, 1);  gpio_put(SEL2_OUT, 1);  gpio_put(SEL3_OUT, 0);
            gpio_set_dir_masked(bus_mask, 0);
            sleep_us(INST_DELAY);
            mem_w_op = (gpio_get_all() & bus_mask) >> BUS_GPIO_START;

            mmu_write(m_adr, mem_w_op);

            gpio_set_dir_masked(bus_mask, 0);
            bus_manager(0);

            if (DEBUG_IO) {
                sprintf(log_buf, "\t\tWRITE 0x%02X -> RAM[0x%04X]\r\n",
                        mem_w_op, m_adr);
                uart_puts(UART_ID, log_buf);
            }
            t_clock = CYC_DATA_XFER;

        } else {
            // Normal T1 — /RD and /WR not asserted yet.
            // Wait until next IRQ to determine cycle type.
            t_clock = CYC_ADDR_READ;
        }
        break;

    // ================================================================
    // CYC_ADDR_READ — /RD or /WR is now asserted: do data transfer
    // ================================================================
    case CYC_ADDR_READ:
        if (rd_low) {
            // ── Memory READ: output data from RAM to Z80 data bus ──
            mem_r_op = mmu_read(m_adr);

            bus_manager(3);              // SEL3=0, DIR3=1 → Pico→Z80
            set_bus_dir(1);             // GPIO outputs
            sleep_us(INST_DELAY);
            gpio_set_dir_masked(bus_mask, bus_mask);
            gpio_put_masked(bus_mask, (uint32_t)mem_r_op << BUS_GPIO_START);
            sleep_us(INST_DELAY);        // bus settling (was sleep_ms!)

            if (DEBUG_IO) {
                sprintf(log_buf, "\t\tREAD  0x%02X -> CPU\r\n", mem_r_op);
                uart_puts(UART_ID, log_buf);
            }
            t_clock = CYC_DATA_XFER;

        } else if (wr_low) {
            // ── Memory WRITE: read data from Z80, store to RAM ─────

            // Enable data transceiver for input (DIR3=0 → Z80→Pico)
            gpio_put(DIR1_OUT, 1);  gpio_put(DIR2_OUT, 1);  gpio_put(DIR3_OUT, 0);
            gpio_put(SEL1_OUT, 1);  gpio_put(SEL2_OUT, 1);  gpio_put(SEL3_OUT, 0);
            gpio_set_dir_masked(bus_mask, 0);
            sleep_us(INST_DELAY);
            mem_w_op = (gpio_get_all() & bus_mask) >> BUS_GPIO_START;

            mmu_write(m_adr, mem_w_op);

            gpio_set_dir_masked(bus_mask, 0);
            bus_manager(0);

            if (DEBUG_IO) {
                sprintf(log_buf, "\t\tWRITE 0x%02X -> RAM[0x%04X]\r\n",
                        mem_w_op, m_adr);
                uart_puts(UART_ID, log_buf);
            }
            t_clock = CYC_DATA_XFER;

        } else {
            // ── Neither /RD nor /WR low → M1 refresh (T4) ──────────
            // Discard the refresh address, go back to IDLE.
            if (DEBUG_IO) {
                uart_puts(UART_ID, "\t\tREFRESH (skip)\r\n");
            }
            t_clock = CYC_IDLE;
        }
        break;

    // ================================================================
    // CYC_DATA_XFER — keep data on bus until /MREQ goes high
    // ================================================================
    case CYC_DATA_XFER:
        // Data stays on the bus.  The next IRQ will see /MREQ high
        // (normal end-of-cycle) and release the bus in the block above.
        // If /MREQ is still low (unexpected), just wait.
        if (DEBUG_IO) {
            uart_puts(UART_ID, "\t\tDATA_XFER (holding)\r\n");
        }
        break;
    }
    }  // end if (mreq_low)
}

// ===========================================================================
// SD card helpers
// ===========================================================================

static FRESULT fr;
static FATFS   fs;
static FIL     fil;
static char    cwdbuf[FF_LFN_BUF] = {0};

char *init_and_mount_sd_card(void) {
    char fr_buf[10];
    memset(&cwdbuf, 0, FF_LFN_BUF);

    if (!spi_configured) {
        show_error_and_halt("SD INIT ERR1");
    }

    fr = f_mount(&fs, "", 1);
    if (fr != FR_OK) {
        sprintf(fr_buf, "SD INIT ERR2 %d", fr);
        show_error_and_halt(fr_buf);
    }

    fr = f_getcwd(cwdbuf, sizeof cwdbuf);
    if (FR_OK != fr) {
        show_error_and_halt("SD INIT ERR3");
    }

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

    if (!spi_configured) {
        print_string(3, 0, "SPI ERROR");
        uart_puts(UART_ID, "SPI ERROR\r\n");
        sleep_ms(DISPLAY_DELAY_LONG);
        skip = true;
    }

    char tmp_buf[10] = "";

    // Mount drive
    if (!skip) {
        fr_local = f_mount(&fs_local, "0", 0);
        if (FR_OK != fr_local) {
            print_string(3, 0, "MOUNT - ERROR");
            uart_puts(UART_ID, "Mount error\r\n");
            sleep_ms(DISPLAY_DELAY_LONG);
            skip = true;
        } else {
            uart_puts(UART_ID, "Mount ok\r\n");
        }
    }

    // Open file for reading
    if (!skip) {
        fr_local = f_open(&fil_local, filename, FA_READ);
        if (fr_local != FR_OK) {
            print_string(0, 0, "OPEN - ERROR");
            uart_puts(UART_ID, "Open error\r\n");
            skip = true;
            while (true)
                ;
        } else {
            uart_puts(UART_ID, "Open ok\r\n");
        }
    }

    while (!skip) {
        if (!f_gets(MACHINE, sizeof(MACHINE), &fil_local)) {
            show_error(0, 0, "INI - MACHINE");
            skip = true;
            break;
        }
        print_line(0, MACHINE);
        sleep_ms(DISPLAY_DELAY_SHORT);

        if (!f_gets(buf, sizeof(buf), &fil_local)) {
            show_error(0, 0, "INI - CANCEL2");
            skip = true;
            break;
        }
        CANCEL2_ADC = decode_hex(buf[0]) * 16 * 16 + decode_hex(buf[1]) * 16 +
                      decode_hex(buf[2]);
        print_line(0, "%CANCEL2: %03x     ", CANCEL2_ADC);
        sleep_ms(DISPLAY_DELAY_SHORT);

        if (!f_gets(buf, sizeof(buf), &fil_local)) {
            show_error(0, 0, "INI - CANCEL");
            skip = true;
            break;
        }
        CANCEL_ADC = decode_hex(buf[0]) * 16 * 16 + decode_hex(buf[1]) * 16 +
                     decode_hex(buf[2]);
        print_line(0, "%CANCEL : %03x     ", CANCEL_ADC);
        sleep_ms(DISPLAY_DELAY_SHORT);

        if (!f_gets(buf, sizeof(buf), &fil_local)) {
            show_error(0, 0, "INI - OK");
            skip = true;
            break;
        }
        OK_ADC = decode_hex(buf[0]) * 16 * 16 + decode_hex(buf[1]) * 16 +
                 decode_hex(buf[2]);
        print_line(0, "%OK     : %03x     ", OK_ADC);
        sleep_ms(DISPLAY_DELAY_SHORT);

        if (!f_gets(buf, sizeof(buf), &fil_local)) {
            show_error(0, 0, "INI - BACK");
            skip = true;
            break;
        }
        BACK_ADC = decode_hex(buf[0]) * 16 * 16 + decode_hex(buf[1]) * 16 +
                   decode_hex(buf[2]);
        print_line(0, "%BACK   : %03x     ", BACK_ADC);
        sleep_ms(DISPLAY_DELAY_SHORT);

        if (!f_gets(buf, sizeof(buf), &fil_local)) {
            show_error(0, 0, "INI - DOWN");
            skip = true;
            break;
        }
        DOWN_ADC = decode_hex(buf[0]) * 16 * 16 + decode_hex(buf[1]) * 16 +
                   decode_hex(buf[2]);
        print_line(0, "%DOWN   : %03x     ", DOWN_ADC);
        sleep_ms(DISPLAY_DELAY_SHORT);

        if (!f_gets(buf, sizeof(buf), &fil_local)) {
            show_error(0, 0, "INI - UP");
            skip = true;
            break;
        }
        UP_ADC = decode_hex(buf[0]) * 16 * 16 + decode_hex(buf[1]) * 16 +
                 decode_hex(buf[2]);
        print_line(0, "%UP     : %03x     ", UP_ADC);
        sleep_ms(DISPLAY_DELAY_SHORT);

        // ----------- PROG1
        if (!f_gets(BANK_PROG[0], sizeof(BANK_PROG[0]), &fil_local)) {
            show_error(0, 0, "INI - PROG1");
            skip = true;
            break;
        }
        print_line(0, "P1: %12s", BANK_PROG[0]);
        sleep_ms(DISPLAY_DELAY_SHORT);

        // ----------- PROG2
        if (!f_gets(BANK_PROG[1], sizeof(BANK_PROG[1]), &fil_local)) {
            show_error(0, 0, "INI - PROG2");
            skip = true;
            break;
        }
        print_line(0, "P2: %12s", BANK_PROG[1]);
        sleep_ms(DISPLAY_DELAY_SHORT);

        // ----------- PROG3
        if (!f_gets(BANK_PROG[2], sizeof(BANK_PROG[2]), &fil_local)) {
            show_error(0, 0, "INI - PROG3");
            skip = true;
            break;
        }
        print_line(0, "P3: %12s", BANK_PROG[2]);
        sleep_ms(DISPLAY_DELAY_SHORT);

        // ----------- PROG4
        if (!f_gets(BANK_PROG[3], sizeof(BANK_PROG[3]), &fil_local)) {
            show_error(0, 0, "INI - PROG4");
            skip = true;
            break;
        }
        print_line(0, "P4: %12s", BANK_PROG[3]);
        sleep_ms(DISPLAY_DELAY_SHORT);

        break;
    }

    if (!skip && f_gets(buf, sizeof(buf), &fil_local)) {
        DEBUG_ADC = buf[0] == '1';
        print_line(0, "%ADC DEBUG: %01x    ", DEBUG_ADC);
        sleep_ms(DISPLAY_DELAY_SHORT);
        clear_screen();
    }

    // Close file
    fr_local = f_close(&fil_local);
    if (fr_local != FR_OK) {
        show_error(0, 0, "INI - CLOSE");
        while (true)
            ;
    }

    // Unmount drive
    f_unmount("0:");
}

// ===========================================================================
// Intel HEX loader
// ===========================================================================

void load_file(bool quiet) {
    reset_hold();

    FRESULT fr_local;
    FATFS   fs_local;
    FIL     fil_local;
    char buf[FILE_BUFF_SIZE];
    char const *p_dir;

    // Only clear the current target bank, not all banks
    clear_bank(cur_bank);

    if (!quiet) {
        clear_screen();
        print_string(0, 0, "Loading HEX");
        print_string(0, 1, file);
        sleep_ms(DISPLAY_DELAY_SHORT);
    }

    p_dir = init_and_mount_sd_card();

    fr_local = f_open(&fil_local, file, FA_READ);
    if (fr_local != FR_OK) {
        sleep_ms(DISPLAY_DELAY_LONG);
        show_error(0, 0, "Can't open file!");
        sleep_ms(DISPLAY_DELAY_LONG);
        show_error(0, 0, file);
        sleep_ms(DISPLAY_DELAY_LONG);
        return;
    }

    // Intel HEX parsing variables
    uint8_t  intel_checksum         = 0;
    uint8_t  intel_byte_count       = 0;
    uint16_t intel_address          = 0;
    uint8_t  intel_address_h        = 0;
    uint8_t  intel_address_l        = 0;
    uint8_t  intel_record_type      = 0;
    uint32_t intel_extended_address = 0;
    uint32_t intel_absolute_address = 0;

    uint8_t parser_state = 0;
    uint8_t hex_digit     = 0;
    uint8_t current_byte  = 0;
    uint8_t data_index    = 0;
    uint8_t data_buffer[MAX_BANKS][RAM_SIZE];

    int      line         = 0;
    uint32_t bytes_loaded = 0;

    if (DEBUG_LOAD) {
        clear_screen();
        print_string(center_string("--HEX LOADER--"), 0, "--HEX LOADER--");
        print_string(0, 1, file);
        print_string(0, 2, "ADDR:          ");
        print_string(0, 3, "BANK:          ");
        print_string(0, 4, "DATA:          ");
        print_string(0, 5, "LINE:          ");
    }

    uart_puts(UART_ID, "Loading hex\r\n");

    while (true) {
        sprintf(serial_text_buffer, "%c\r", load_chars[load_char_index]);
        uart_puts(UART_ID, serial_text_buffer);
        load_char_index += 1;
        if (load_char_index > strlen(load_chars)) {
            load_char_index = 0;
        }

        memset(&buf, 0, sizeof(buf));

        if (!f_gets(buf, sizeof(buf), &fil_local))
            break;

        int i = 0;

        // Skip empty lines
        if (buf[0] == '\0' || buf[0] == '\r' || buf[0] == '\n')
            continue;

        // Check for Intel HEX start character
        if (buf[0] != ':') {
            sprintf(text_buffer, "Not HEX Line: %d Data: %016x", line, buf);
            uart_puts(UART_ID, text_buffer);
            clear_screen();
            fr_local = f_close(&fil_local);
            return;
        }

        // Reset parser for new line
        parser_state       = 1;
        intel_byte_count   = 0;
        intel_address      = 0;
        intel_address_h    = 0;
        intel_address_l    = 0;
        intel_record_type  = 0;
        intel_checksum     = 0;
        hex_digit          = 0;
        current_byte       = 0;
        data_index         = 0;

        cur_bank = 0;

        i = 1; // Skip the ':'

        while (true) {
            byte b = buf[i++];

            if (!b || b == '\n' || b == '\r')
                break;

            if (b == ' ' || b == '\t')
                continue;

            int decoded = decode_hex(b);

            if (decoded == -1) {
                sprintf(text_buffer, "Invalid HEX: Line %05d", line);
                uart_puts(UART_ID, text_buffer);
                clear_screen();
                fr_local = f_close(&fil_local);
                return;
            }

            if (hex_digit == 0) {
                current_byte = decoded * 16;
                hex_digit = 1;
            } else {
                current_byte += decoded;
                hex_digit = 0;

                if (parser_state < 6) {
                    intel_checksum += current_byte;
                }

                switch (parser_state) {
                case 1: // Byte count
                    intel_byte_count = current_byte;
                    parser_state = 2;
                    break;

                case 2: // Address high byte
                    cur_bank = (current_byte & 0b11000000) >> 6;
                    intel_address = current_byte << 8;
                    parser_state = 3;
                    break;

                case 3: // Address low byte
                    intel_address |= current_byte;
                    parser_state = 4;
                    break;

                case 4: // Record type
                    intel_record_type = current_byte;
                    if (intel_byte_count == 0) {
                        parser_state = 6;
                    } else {
                        parser_state = 5;
                        data_index = 0;
                    }
                    break;

                case 5: // Data bytes
                    data_buffer[cur_bank][data_index++] = current_byte;
                    if (data_index >= intel_byte_count) {
                        parser_state = 6;
                    }
                    break;

                case 6: // Checksum
                    if ((intel_checksum + current_byte) & 0xFF) {
                        sprintf(text_buffer, "Checksum err Line %05d", line);
                        uart_puts(UART_ID, text_buffer);
                        clear_screen();
                        fr_local = f_close(&fil_local);
                        return;
                    }

                    switch (intel_record_type) {
                    case 0: // Data record
                        intel_absolute_address =
                            intel_extended_address + intel_address;

                        for (int j = 0; j < intel_byte_count; j++) {
                            ram[cur_bank][intel_absolute_address + j] =
                                data_buffer[cur_bank][j];
                            bytes_loaded++;

                            if (DEBUG_LOAD && j < 4) {
                                sprintf(text_buffer, "%02X",
                                        data_buffer[cur_bank][j]);
                                print_string(7 + (j * 2), 4, text_buffer);
                            }
                        }

                        if (DEBUG_LOAD) {
                            sprintf(text_buffer, "%08lX",
                                    intel_absolute_address);
                            print_string(7, 2, text_buffer);

                            sprintf(text_buffer, "%08lX", cur_bank);
                            print_string(7, 3, text_buffer);

                            sprintf(text_buffer, "%08d", line);
                            print_string(7, 5, text_buffer);
                            sleep_ms(5);
                        }
                        break;

                    case 1: // EOF
                        if (DEBUG_LOAD) {
                            print_string(0, 5, "EOF reached      ");
                        }
                        break;

                    case 2: // Extended segment address
                        if (intel_byte_count == 2) {
                            intel_extended_address =
                                (data_buffer[cur_bank][0] << 8 |
                                 data_buffer[cur_bank][1]) * 16;
                            if (DEBUG_LOAD) {
                                sprintf(text_buffer, "SegAddr: %08lX",
                                        intel_extended_address);
                                print_string(0, 5, text_buffer);
                            }
                        }
                        break;

                    case 3: // Start segment address (8086) — ignore
                        break;

                    case 4: // Extended linear address
                        if (intel_byte_count == 2) {
                            intel_extended_address =
                                (data_buffer[cur_bank][0] << 8 |
                                 data_buffer[cur_bank][1]) << 16;
                            if (DEBUG_LOAD) {
                                sprintf(text_buffer, "LinAddr: %08lX",
                                        intel_extended_address);
                                print_string(0, 5, text_buffer);
                            }
                        }
                        break;

                    case 5: // Start linear address (80386+) — ignore
                        break;

                    default:
                        sprintf(text_buffer, "Unknown type %02X",
                                intel_record_type);
                        print_string(0, 5, text_buffer);
                        break;
                    }

                    parser_state = 0;
                    break;
                }
            }
        }
        line++;

        if (intel_record_type == 1) {
            break;
        }
    }

    fr_local = f_close(&fil_local);

    if (fr_local != FR_OK) {
        show_error(0, 0, "Can't close file!");
    }

    f_unmount("0:");

    if (DEBUG_LOAD) {
        sprintf(text_buffer, "Loaded %lu bytes", bytes_loaded);
        uart_puts(UART_ID, text_buffer);
        uart_puts(UART_ID, "\r\n");
        print_string(0, 6, text_buffer);
        sleep_ms(DISPLAY_DELAY_LONG);
    }

    if (!quiet) {
        clear_screen();
        sprintf(text_buffer, "Loaded: %lu bytes", bytes_loaded);
        print_string(0, 0, text_buffer);
        print_string(0, 1, file);
        sleep_ms(DISPLAY_DELAY);
    }

    strcpy(BANK_PROG[cur_bank], file);
    reset_release();
    return;
}

// ===========================================================================
// Load from SD card (with file selector)
// ===========================================================================

void load(void) {
    int aborted = select_file();
    clear_screen();

    if (aborted == -1) {
        print_string(0, 3, "CANCELED!       ");
        sleep_ms(DISPLAY_DELAY);
        sleep_ms(DISPLAY_DELAY);
        return;
    }

    load_file(true);
}

// ===========================================================================
// Load initial programs from Z80NEO.INI list
// ===========================================================================

void load_init_progs(void) {
    cur_bank = 0;
    if (BANK_PROG[0][0] >= 48) {
        print_string(0, 3, "LOAD PROG 0");
        sleep_ms(DISPLAY_DELAY);
        sleep_ms(DISPLAY_DELAY);
        strcpy(file, BANK_PROG[0]);
        load_file(true);
    }

    cur_bank = 1;
    if (BANK_PROG[1][0] >= 48) {
        print_string(0, 3, "LOAD PROG 1");
        sleep_ms(DISPLAY_DELAY);
        sleep_ms(DISPLAY_DELAY);
        strcpy(file, BANK_PROG[1]);
        load_file(true);
    }

    cur_bank = 2;
    if (BANK_PROG[2][0] >= 48) {
        print_string(0, 3, "LOAD PROG 2");
        sleep_ms(DISPLAY_DELAY);
        sleep_ms(DISPLAY_DELAY);
        strcpy(file, BANK_PROG[2]);
        load_file(true);
    }

    cur_bank = 3;
    if (BANK_PROG[3][0] >= 48) {
        print_string(0, 3, "LOAD PROG 3");
        sleep_ms(DISPLAY_DELAY);
        sleep_ms(DISPLAY_DELAY);
        strcpy(file, BANK_PROG[3]);
        load_file(true);
    }

    cur_bank = 0;
}

// ===========================================================================
// Load Intel HEX from UART
// ===========================================================================

void load_hex_from_uart(void) {
    reset_hold();

    // Clear all banks
    cur_bank = 0;
    for (uint8_t pgm = 0; pgm < MAX_BANKS; pgm++) {
        clear_bank(pgm);
    }

    // Disable UART RX interrupt — we'll poll the FIFO directly
    uart_set_irq_enables(UART_ID, false, false);
    // Drain stale data
    while (uart_is_readable(UART_ID)) uart_getc(UART_ID);

    clear_screen();
    print_string(0, 0, "UART HEX Load");
    print_string(0, 1, "Send file now...");
    print_string(0, 7, "CANCEL to abort");

    // Intel HEX parsing state
    uint8_t  intel_checksum         = 0;
    uint8_t  intel_byte_count       = 0;
    uint16_t intel_address          = 0;
    uint8_t  intel_record_type      = 0;
    uint32_t intel_extended_address = 0;
    uint32_t intel_absolute_address = 0;
    uint32_t bytes_loaded           = 0;

    uint8_t data_buffer[MAX_BANKS][RAM_SIZE];

    char    line_buf[128];
    uint8_t line_pos   = 0;
    bool    eof        = false;
    int     line_count = 0;

    while (!eof) {
        // Check for abort
        if (read_button_state() == CANCEL) {
            wait_for_button_release();
            clear_screen();
            print_string(0, 3, "ABORTED");
            sleep_ms(DISPLAY_DELAY_LONG);
            goto done;
        }

        if (!uart_is_readable(UART_ID)) {
            sleep_ms(1);
            continue;
        }

        char c = uart_getc(UART_ID);

        if (c == '\r') continue;

        if (c == '\n') {
            if (line_pos == 0) continue;  // empty line

            line_buf[line_pos] = '\0';
            line_pos = 0;
            line_count++;

            // Must start with ':'
            if (line_buf[0] != ':') {
                sprintf(text_buffer, "Bad line %d", line_count);
                print_string(0, 3, text_buffer);
                sleep_ms(DISPLAY_DELAY);
                goto done;
            }

            // Parse one Intel HEX line
            uint8_t parser_state = 1;  // 1=count,2=addrH,3=addrL,4=type,5=data,6=chk
            uint8_t hex_digit    = 0;
            uint8_t current_byte = 0;
            uint8_t data_index   = 0;

            intel_checksum   = 0;
            intel_byte_count = 0;
            intel_address    = 0;
            intel_record_type = 0;
            cur_bank = 0;

            for (int i = 1; i < line_pos; i++) {
                int decoded = decode_hex(line_buf[i]);
                if (decoded == -1) {
                    sprintf(text_buffer, "Hex err L%d", line_count);
                    print_string(0, 3, text_buffer);
                    sleep_ms(DISPLAY_DELAY);
                    goto done;
                }

                if (hex_digit == 0) {
                    current_byte = decoded * 16;
                    hex_digit = 1;
                } else {
                    current_byte += decoded;
                    hex_digit = 0;

                    if (parser_state < 6)
                        intel_checksum += current_byte;

                    switch (parser_state) {
                    case 1: // byte count
                        intel_byte_count = current_byte;
                        parser_state = 2;
                        break;
                    case 2: // address high
                        cur_bank = (current_byte & 0b11000000) >> 6;
                        intel_address = current_byte << 8;
                        parser_state = 3;
                        break;
                    case 3: // address low
                        intel_address |= current_byte;
                        parser_state = 4;
                        break;
                    case 4: // record type
                        intel_record_type = current_byte;
                        parser_state = (intel_byte_count == 0) ? 6 : 5;
                        data_index = 0;
                        break;
                    case 5: // data
                        data_buffer[cur_bank][data_index++] = current_byte;
                        if (data_index >= intel_byte_count)
                            parser_state = 6;
                        break;
                    case 6: // checksum
                        if ((intel_checksum + current_byte) & 0xFF) {
                            sprintf(text_buffer, "Chk err L%d", line_count);
                            print_string(0, 3, text_buffer);
                            sleep_ms(DISPLAY_DELAY);
                            goto done;
                        }

                        switch (intel_record_type) {
                        case 0: // data
                            intel_absolute_address = intel_extended_address + intel_address;
                            for (int j = 0; j < intel_byte_count; j++) {
                                ram[cur_bank][intel_absolute_address + j] =
                                    data_buffer[cur_bank][j];
                                bytes_loaded++;
                            }
                            break;
                        case 1: // EOF
                            eof = true;
                            break;
                        case 2:
                            if (intel_byte_count == 2)
                                intel_extended_address =
                                    (data_buffer[cur_bank][0] << 8 |
                                     data_buffer[cur_bank][1]) * 16;
                            break;
                        case 4:
                            if (intel_byte_count == 2)
                                intel_extended_address =
                                    (data_buffer[cur_bank][0] << 8 |
                                     data_buffer[cur_bank][1]) << 16;
                            break;
                        default:
                            break;
                        }
                        break;
                    }
                }
            }

            // Progress
            if ((line_count & 0x0F) == 0) {
                sprintf(text_buffer, "Lines: %d", line_count);
                print_string(0, 2, text_buffer);
            }
        } else {
            if (line_pos < sizeof(line_buf) - 1)
                line_buf[line_pos++] = c;
        }
    }

done:
    // Re-enable UART RX interrupt
    uart_set_irq_enables(UART_ID, true, false);

    clear_screen();
    sprintf(text_buffer, "Loaded: %lu bytes", bytes_loaded);
    print_string(0, 0, text_buffer);
    print_string(0, 1, "%d lines", line_count);
    sleep_ms(DISPLAY_DELAY_LONG);

    reset_release();
}

// ===========================================================================
// Save to SD card
// ===========================================================================

void save(void) {
    for (uint32_t b = 0; b < RAM_SIZE; b++) {
        sdram[b] = ram[cur_bank][b];
    }

    clear_screen();

    int aborted = create_name();

    if (aborted == -1) {
        print_string(0, 3, "CANCELED!       ");
        sleep_ms(DISPLAY_DELAY);
        sleep_ms(DISPLAY_DELAY);
        return;
    }

    print_string(0, 0, "Saving");
    print_string(0, 1, file);

    FRESULT fr_local;
    FATFS   fs_local;
    FIL     fil_local;
    int     ret;
    char    buf[100];
    char const *p_dir;

    p_dir = init_and_mount_sd_card();

    fr_local = f_open(&fil_local, file, FA_READ);
    if (FR_OK == fr_local) {
        print_string(0, 2, "Overwrite File?");
        if (!wait_for_yes_no_button()) {
            print_string(0, 3, "*** CANCELED ***");
            sleep_ms(DISPLAY_DELAY);
            sleep_ms(DISPLAY_DELAY);
            f_close(&fil_local);
            return;
        } else
            print_string(0, 2, "FILE Open      ");
        f_close(&fil_local);
    }

    // Open file for writing (create always)
    fr_local = f_open(&fil_local, file, FA_WRITE | FA_CREATE_ALWAYS);
    if (fr_local != FR_OK) {
        show_error(0, 0, "WRITE ERROR 1");
        f_close(&fil_local);
        return;
    }

	    // Write Intel HEX format (16 bytes per data record)
	    for (uint32_t base = 0; base < RAM_SIZE; base += 16) {
	        uint8_t count = 16;
	        if (base + 16 > RAM_SIZE)
	            count = RAM_SIZE - base;

	        uint8_t checksum = count + (base >> 8) + (base & 0xFF);

	        ret = f_printf(&fil_local, ":%02X%04X00", count, (unsigned int)base);
	        if (ret < 0) goto write_err;

	        for (uint8_t i = 0; i < count; i++) {
	            uint8_t b = sdram[base + i];
	            checksum += b;
	            ret = f_printf(&fil_local, "%02X", b);
	            if (ret < 0) goto write_err;
	        }

	        checksum = (~checksum + 1) & 0xFF;
	        ret = f_printf(&fil_local, "%02X\n", checksum);
	        if (ret < 0) goto write_err;
	    }

	    // End-of-file record
	    ret = f_printf(&fil_local, ":00000001FF\n");
	    if (ret < 0) goto write_err;

	    goto close_file;

write_err:
	    show_error(0, 0, "WRITE ERROR 2");
	    f_close(&fil_local);
	    return;

close_file:

    fr_local = f_close(&fil_local);

    if (fr_local != FR_OK) {
        show_error_wait_for_button("CANT'T CLOSE FILE");
        clear_screen();
    } else {
        strcpy(BANK_PROG[cur_bank], file);
        print_string(0, 3, "Saved: %s", file);
        sleep_ms(DISPLAY_DELAY);
        sleep_ms(DISPLAY_DELAY);
    }

    return;
}
