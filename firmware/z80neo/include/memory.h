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
#define SD_RAM_SIZE    16384
#define MAX_BANKS      4

// ---------------------------------------------------------------------------
// Debug switches
// ---------------------------------------------------------------------------

#define DEBUG_LOAD true
#define DEBUG_IO   false
#define ADC_DEBUG_DELAY 100

extern volatile bool DEBUG_ADC;

// ---------------------------------------------------------------------------
// UART
// ---------------------------------------------------------------------------

#define UART_ID    uart0
#define BAUD_RATE  115200
#define DATA_BITS  8
#define STOP_BITS  1
#define PARITY     UART_PARITY_NONE

// UART RX buffer
#define UART_BUF_SIZE 8

// ---------------------------------------------------------------------------
// MMU
// ---------------------------------------------------------------------------

#define MMU_PAGE_0 0xF0
#define MMU_PAGE_1 0xF1
#define MMU_PAGE_2 0xF2
#define MMU_PAGE_3 0xF3

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
#define INST_DELAY 10       // μs bus settling (0 for ≥100 kHz)

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

#define DIR1_OUT  29
#define DIR2_OUT  30
#define DIR3_OUT  31

#define GPIO_PWM_SIG 32

#define RESET_OUT 33

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
extern uint8_t mmuram[(uint16_t)RAM_SIZE];
extern uint8_t sdram[(uint16_t)SD_RAM_SIZE];

// Current bank
extern volatile uint8_t cur_bank;

// CPU state
extern uint16_t pc;
extern uint8_t  opcode;

extern volatile uint8_t t_clock;
extern volatile uint8_t m_clock;

extern uint8_t next_oclock;
extern uint8_t next_mclock;
extern uint8_t next_dclock;

// Address
extern volatile uint8_t  low_adr;
extern volatile uint8_t  high_adr;
extern volatile uint16_t m_adr;

// I/O ops
extern volatile uint8_t io_r_op;
extern volatile uint8_t io_w_op;
extern volatile uint8_t opcode_r_op;
extern volatile uint8_t mem_r_op;
extern volatile uint8_t mem_w_op;

// UART state
extern char    rx_buffer[UART_BUF_SIZE];
extern uint8_t rx_count;
extern uint8_t rx_index;
extern uint8_t rx_read;
extern bool    rx_data_available;

extern volatile uint8_t serial_status_1;
extern volatile uint8_t serial_status_2;

// UART TX buffer (non-blocking send from IRQ context)
#define UART_TX_BUF_SIZE 64
extern uint8_t  tx_buffer[UART_TX_BUF_SIZE];
extern uint8_t  tx_head;
extern uint8_t  tx_tail;
extern uint8_t  tx_count;

// Debug counters
extern uint32_t d_adr;
extern uint32_t dr_op;
extern uint32_t dw_op;

// Bus control
extern bool mreq;
extern bool iorq;
extern bool rd;
extern bool wr;
extern bool clk_level;

extern bool mreq_status;
extern bool iorq_status;

extern uint32_t bus_mask;

extern volatile bool disabled;
extern volatile bool read;
extern volatile bool written;

// ADC thresholds
extern volatile uint16_t CANCEL2_ADC;
extern volatile uint16_t CANCEL_ADC;
extern volatile uint16_t OK_ADC;
extern volatile uint16_t BACK_ADC;
extern volatile uint16_t DOWN_ADC;
extern volatile uint16_t UP_ADC;

// SPI
extern bool spi_configured;

// PWM
extern uint slice;

// Misc
extern char   MACHINE[FILE_LENGTH];
extern char   BANK_PROG[4][FILE_LENGTH];
extern uint8_t in_bytes[1024];
extern char    log_buf[32];

// ---------------------------------------------------------------------------
// Function prototypes
// ---------------------------------------------------------------------------

// Hex decoding
unsigned char decode_hex(char c);

// Bank management
void clear_bank(uint8_t bank);

// Reset
void reset_release(void);
void reset_hold(void);

// Bus
void set_bus_dir(int direction);
void bus_manager(int value);
void nop_delay(void);

// UART
void on_uart_rx(void);
char read_uart_char(void);
void uart_status_handler(void);

// GPIO / bus callbacks
void bus_callback(uint pin, uint32_t events);

// PWM / IRQ handler
void pwm_irq_handler(void);

// SD card
int   sd_read_init(void);
char *init_and_mount_sd_card(void);

// File operations
void load_file(bool quiet);
void load(void);
void load_hex_from_uart(void);
void save(void);
void load_init_progs(void);

#endif // MEMORY_H
