#ifndef MEMORY_H
#define MEMORY_H

#include <stdbool.h>
#include <stdint.h>

// ---------------------------------------------------------------------------
// System constants
// ---------------------------------------------------------------------------

#define VERSION "v0.6.3 - ALPHA"

#define FILE_LENGTH    17
#define FILE_BUFF_SIZE 1024
#define FILE_EXT       "*.HEX"

#define RAM_SIZE       16384
#define MAX_BANKS      4
#define PSRAM_BANKS    4
#define TOTAL_BANKS    (MAX_BANKS + PSRAM_BANKS)

// ---------------------------------------------------------------------------
// Debug switches
// ---------------------------------------------------------------------------

#define DEBUG_LOAD true
#define DEBUG_IO   true
#define ADC_DEBUG_DELAY 100

extern volatile bool DEBUG_ADC;

// Z80 clock frequency in Hz (defined in main.c)
extern float CPU_SPEED;

// ---------------------------------------------------------------------------
// UART
// ---------------------------------------------------------------------------

#define UART_ID    uart0
#define BAUD_RATE  115200
#define DATA_BITS  8
#define STOP_BITS  1
#define PARITY     UART_PARITY_NONE

// UART RX buffer
#define UART_BUF_SIZE 256

// ---------------------------------------------------------------------------
// MMU
// ---------------------------------------------------------------------------

// IO ports for the 4 virtual pages (16 KB each)
#define MMU_PAGE_0 0xF0
#define MMU_PAGE_1 0xF1
#define MMU_PAGE_2 0xF2
#define MMU_PAGE_3 0xF3

// OS physical page range: 0x20–0x3F (32 pages × 16 KB = 512 KB)
#define MMU_PHYS_START 0x20
#define MMU_PHYS_PAGES 32

// ---------------------------------------------------------------------------
// SIO
// ---------------------------------------------------------------------------

#define SERIAL_PORT_1   0x80
#define SERIAL_STATUS_1 0x81
#define SERIAL_PORT_2   0x90
#define SERIAL_STATUS_2 0x91

// ---------------------------------------------------------------------------
// Clock
// ---------------------------------------------------------------------------

#define PWM_WRAP   65535   // 16-bit resolution
#define INST_DELAY 0       // μs bus settling (0 for ≥500 kHz)

// ---------------------------------------------------------------------------
// GPIO pin definitions
// ---------------------------------------------------------------------------

#define UART_TX_PIN      0
#define UART_RX_PIN      1

#define PICO_I2C_SDA_PIN 2
#define PICO_I2C_SCL_PIN 3

#define PIN_SPI1_CS   9
#define PIN_SPI1_SCK  10
#define PIN_SPI1_MOSI 11

#define BUS_GPIO_START 12
#define BUS_GPIO_END   19

#define MREQ_INPUT 20
#define RD_INPUT   21
#define IORQ_INPUT 22
#define WR_INPUT   23

#define PIN_SPI1_MISO 24

#define LED_PIN 25

#define SEL1_OUT  26   // ADDRESS LOW
#define SEL2_OUT  27   // ADDRESS HIGH
#define SEL3_OUT  28   // DATA

#define DIR3_OUT  29   // DATA direction (swapped with DIR1)
#define DIR2_OUT  30
#define DIR1_OUT  31   // ADDR LOW direction (swapped with DIR3)

#define GPIO_PWM_SIG 32   // Z80 clock output (GPIO32 → Z80 CLK)

#define RESET_OUT 33      // Z80 reset output (GPIO33 → Z80 RESET)

#define ADC_KEYS_INPUT 40   // ADC KEYS

#define PICO_I2C_INSTANCE i2c1

// ---------------------------------------------------------------------------
// byte type alias
// ---------------------------------------------------------------------------

#define byte uint8_t

// ---------------------------------------------------------------------------
// External globals
// ---------------------------------------------------------------------------

// RAM banks
extern uint8_t ram[MAX_BANKS][(uint16_t)RAM_SIZE];

// PSRAM base (NULL if unavailable)
extern uint8_t *psram_base;

// Current bank
extern volatile uint8_t cur_bank;

// Address
extern volatile uint16_t m_adr;

// I/O ops
extern volatile uint8_t io_r_op;
extern volatile uint8_t io_w_op;
extern volatile uint8_t mem_r_op;
extern volatile uint8_t mem_w_op;

// UART state
extern char     rx_buffer[UART_BUF_SIZE];
extern uint16_t rx_count;
extern uint16_t rx_index;
extern uint16_t rx_read;
extern bool     rx_data_available;

extern volatile uint8_t serial_status_1;

// UART TX buffer (non-blocking send from IRQ context)
#define UART_TX_BUF_SIZE 4096
extern uint8_t   tx_buffer[UART_TX_BUF_SIZE];
extern uint16_t  tx_head;
extern uint16_t  tx_tail;
extern uint16_t  tx_count;

// Debug counters
extern uint32_t d_adr;
extern uint32_t dr_op;
extern uint32_t dw_op;
extern uint32_t io_op;

// Bus control
extern uint32_t bus_mask;

extern volatile bool disabled;
extern volatile bool system_up;

// ADC thresholds
extern volatile uint16_t CANCEL2_ADC;
extern volatile uint16_t CANCEL_ADC;
extern volatile uint16_t OK_ADC;
extern volatile uint16_t BACK_ADC;
extern volatile uint16_t DOWN_ADC;
extern volatile uint16_t UP_ADC;

// SPI
extern bool spi_configured;

// Misc
extern char   MACHINE[FILE_LENGTH];
extern char   BANK_PROG[4][FILE_LENGTH];

// ---------------------------------------------------------------------------
// Function prototypes
// ---------------------------------------------------------------------------

// Hex decoding — returns 0..15, or -1 for a non-hex character
int decode_hex(char c);

// Bank management
void clear_bank(uint8_t bank);

// Reset
void reset_release(void);
void reset_hold(void);

// Bus
void set_bus_dir(int direction);

// UART
char read_uart_char(void);
void uart_rx_poll(void);
void echo_track_tx(uint8_t c);
void uart_status_handler(void);

// MMU accessors (used by PIO bus handler)
uint8_t mmu_read(uint16_t addr);
void mmu_write(uint16_t addr, uint8_t data);

// I/O accessors (used by PIO IORQ handler)
uint8_t io_read_port(uint8_t port);
void io_write_port(uint8_t port, uint8_t data);

// SD card
int   sd_read_init(void);
char *init_and_mount_sd_card(void);

// File operations
void load_file(bool quiet);
void load(void);
void load_init_progs(void);

// Boot-time sanity dump (UART)
void post_dump(void);

#endif // MEMORY_H
