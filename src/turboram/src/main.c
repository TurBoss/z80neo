#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include <bsp/board_api.h>
#include <tusb.h>

// Pico2
#include <hardware/adc.h>
#include <hardware/clocks.h>
#include <hardware/gpio.h>
#include <hardware/pwm.h>
#include <hardware/spi.h>
#include <hardware/vreg.h>
#include <pico/multicore.h>
#include <pico/stdlib.h>
#include <pico/time.h>

// Screen
#include "ssd1306_i2c.h"

// SD Card
#include "ff.h"
#include "tf_card.h"

#undef CLK_SLOW_DEFAULT
#undef CLK_FAST_DEFAULT

#define CLK_SLOW_DEFAULT (100 * KHZ)
#define CLK_FAST_DEFAULT (10 * MHZ)

bool spi_configured;

struct render_area frame_area = {
	start_col : 0,
	end_col : SSD1306_WIDTH - 1,
	start_page : 0,
	end_page : SSD1306_NUM_PAGES - 1
};

// Set PRE_ALLOCATE true to pre-allocate file clusters.
const bool PRE_ALLOCATE = true;

// Set SKIP_FIRST_LATENCY true if the first read/write to the SD can
// be avoid by writing a file header or reading the first record.
const bool SKIP_FIRST_LATENCY = true;

// Size of read/write.
#define BUF_SIZE 512

// File size in MB where MB = 1,000,000 bytes.
const uint32_t FILE_SIZE_MB = 5;

// Write pass count.
const uint8_t WRITE_COUNT = 2;

// Read pass count.
const uint8_t READ_COUNT = 2;

//==============================================================================
// End of configuration constants.
//------------------------------------------------------------------------------
// File size in bytes.
const uint32_t FILE_SIZE = 1000000UL * FILE_SIZE_MB;

// Insure 4-byte alignment.

uint32_t buf32[(BUF_SIZE + 3) / 4];

uint8_t *buf = (uint8_t *)buf32;

uint8_t *disp_buf_0 = (uint8_t *)buf32;
uint8_t *disp_buf_1 = (uint8_t *)buf32;
uint8_t *disp_buf_2 = (uint8_t *)buf32;
uint8_t *disp_buf_3 = (uint8_t *)buf32;

void show_info(void);

void reset_release(void);
void reset_hold(void);

void show_error_and_halt(char *err);
void show_error(int b, int a, char *err);
void show_error_wait_for_button(char *err);

char *init_and_mount_sd_card(void);

void load_file(bool quiet);

void pgm1();
void pgm2();

//
//
//

#define VERSION "  v0.1 - alpha  "

//
// ADC Configuration (MPF.INI file!)
//

#define FILE_LENGTH 17
#define FILE_BUFF_SIZE 64
#define FILE_EXT "*.HEX"

//
//
//

#define RAM_SIZE 32768
#define SD_RAM_SIZE 32768

//
//
//

#define DEBUG_LOAD false
#define ADC_DEBUG_DELAY 100

volatile bool DEBUG_ADC = false;

volatile char MACHINE[FILE_LENGTH] = "Z80";
volatile char BANK_PROG[8][FILE_LENGTH];

volatile uint16_t CANCEL2_ADC = 0xFFF;
volatile uint16_t CANCEL_ADC = 0x900;
volatile uint16_t OK_ADC = 0x7C0;
volatile uint16_t BACK_ADC = 0x510;
volatile uint16_t DOWN_ADC = 0x220;
volatile uint16_t UP_ADC = 0x100;

//
//
//

#define byte uint8_t

#define DISPLAY_DELAY 200
#define DISPLAY_DELAY_LONG 500
#define DISPLAY_DELAY_SHORT 100

#define BLINK_DELAY (100 * 1000)
#define LONG_BUTTON_DELAY (400 * 1000)

typedef char display_line[17];
display_line file;

const char *hexStringChar[] = {"0", "1", "2", "3", "4", "5", "6", "7",
							   "8", "9", "a", "b", "C", "d", "E", "F"};

#define TEXT_BUFFER_SIZE 256

char text_buffer[TEXT_BUFFER_SIZE];

char tbmon_text_buffer[4][17];

char line1[TEXT_BUFFER_SIZE];
char line2[TEXT_BUFFER_SIZE];
char line3[TEXT_BUFFER_SIZE];
char line4[TEXT_BUFFER_SIZE];

#define BYTES_PER_ROW 4
#define BYTES_PER_LINE 16
#define LINES 4

char *screen[LINES] = {line1, line2, line3, line4};

//
// DEFINE GPIO
//

#define BUS_GPIO_START 0
#define BUS_GPIO_END 8

#define SEL1_OUT 8	// ADDRESS LOW
#define SEL2_OUT 9	// ADDRESS HIGH
#define SEL3_OUT 10 // DATA

#define DIR1_OUT 11
#define DIR2_OUT 12
#define DIR3_OUT 13

#define MREQ_INPUT 14
#define RD_INPUT 15

#define PIN_SPI1_MISO   16
#define PIN_SPI1_SCK    17
#define PIN_SPI1_CS     18
#define PIN_SPI1_MOSI   19

#define PICO_DEFAULT_I2C_SDA_PIN 20
#define PICO_DEFAULT_I2C_SCL_PIN 21

#define GPIO_PWM_SIG 22

#define IORQ_INPUT 26
#define WR_INPUT 27

#define ADC_KEYS_INPUT 28


const uint8_t LED_PIN = PICO_DEFAULT_LED_PIN;

//
//
//

#define MAX_BANKS 8

static uint8_t cur_bank = 0;

uint8_t ram[MAX_BANKS][(uint32_t)RAM_SIZE] = {};
uint8_t sdram[(uint32_t)SD_RAM_SIZE] = {};

uint16_t tbmon_idx = 0;

//
//
//

volatile bool tbmon = false;
volatile bool tbmon_loaded = false;

volatile bool disabled = false;
volatile bool read = false;
volatile bool written = false;
volatile bool confirmed = false;


uint8_t bus_mask = 0;
uint32_t gpio = 0;

uint8_t low_adr = 0;
uint16_t high_adr = 0;

uint16_t m_adr = 0;
uint8_t r_op = 0;
uint8_t w_op = 0;

uint32_t d_adr = 0;
uint32_t dr_op = 0;
uint32_t dw_op = 0;

//
// Utilities
//

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

// unused

unsigned char reverse_bits(unsigned char b) {
	return (b & 0b00000001) << 3 | (b & 0b00000010) << 1 |
		   (b & 0b00000100) >> 1 | (b & 0b00001000) >> 3 |
		   (b & 0b00010000) << 3 | (b & 0b00100000) << 1 |
		   (b & 0b01000000) >> 1 | (b & 0b10000000) >> 3;
}


void clear_bank(uint8_t bank) {
	for (uint32_t adr = 0; adr < RAM_SIZE; adr++) {
		ram[bank][adr] = 0;
	}
	memset(BANK_PROG[bank], 0, FILE_LENGTH);
}

//
// Display
//

void clear_screen() {
	memset(buf, 0, SSD1306_BUF_LEN);
	render(buf, &frame_area);
	memset(line1, 0, TEXT_BUFFER_SIZE);
	memset(line2, 0, TEXT_BUFFER_SIZE);
	memset(line3, 0, TEXT_BUFFER_SIZE);
	memset(line4, 0, TEXT_BUFFER_SIZE);
}

void clear_screen0() {
	memset(buf, 0, SSD1306_BUF_LEN);
	memset(line1, 0, TEXT_BUFFER_SIZE);
	memset(line2, 0, TEXT_BUFFER_SIZE);
	memset(line3, 0, TEXT_BUFFER_SIZE);
	memset(line4, 0, TEXT_BUFFER_SIZE);
}

void clear_line0(int line) {
	switch (line) {
	case 0:
		memset(line1, 0, TEXT_BUFFER_SIZE);
		break;
	case 1:
		memset(line2, 0, TEXT_BUFFER_SIZE);
		break;
	case 2:
		memset(line3, 0, TEXT_BUFFER_SIZE);
		break;
	case 4:
		memset(line4, 0, TEXT_BUFFER_SIZE);
		break;
	}
	strcpy(screen[line], "                ");
	WriteString(buf, 0, line * 8, screen[line]);
}

void clear_line(int line) {
	clear_line0(line);
	render(buf, &frame_area);
}

void print_string0(int x, int y, char *text, ...) {

	va_list args;
	va_start(args, text);
	vsnprintf(text_buffer, TEXT_BUFFER_SIZE, text, args);
	strcpy(screen[y], text_buffer);
	WriteString(buf, x * 8, y * 8, text_buffer);
	va_end(args);
}

void print_string(int x, int y, char *text, ...) {
	va_list args;
	va_start(args, text);
	vsnprintf(text_buffer, TEXT_BUFFER_SIZE, text, args);
	strcpy(screen[y], text_buffer);
	WriteString(buf, x * 8, y * 8, text_buffer);
	render(buf, &frame_area);
}

void print_line(int x, char *text, ...) {
	va_list args;
	va_start(args, text);
	vsnprintf(text_buffer, TEXT_BUFFER_SIZE, text, args);
	char *screen0 = screen[0];
	for (int i = 0; i < 3; i++) {
		screen[i] = screen[i + 1];
		WriteString(buf, x * 8, i * 8, screen[i]);
	}
	screen[3] = screen0;
	strcpy(screen[3], text_buffer);
	WriteString(buf, x * 8, 3 * 8, text_buffer);
	render(buf, &frame_area);
	va_end(args);
}

void print_line0(int x, char *text, ...) {
	va_list args;
	va_start(args, text);
	vsnprintf(text_buffer, TEXT_BUFFER_SIZE, text, args);
	char *screen0 = screen[0];
	for (int i = 0; i < 3; i++) {
		screen[i] = screen[i + 1];
		WriteString(buf, x * 8, i * 8, screen[i]);
	}
	screen[3] = screen0;
	strcpy(screen[3], text_buffer);
	WriteString(buf, x * 8, 3 * 8, text_buffer);
	va_end(args);
}

void print_char0(int x, int y, char c) {
	text_buffer[0] = c;
	text_buffer[1] = 0;
	WriteString(buf, x * 8, y * 8, text_buffer);
}

void print_char(int x, int y, char c) {
	text_buffer[0] = c;
	text_buffer[1] = 0;
	WriteString(buf, x * 8, y * 8, text_buffer);
	render(buf, &frame_area);
}

void disp_plot0(int x, int y) { SetPixel(buf, x, y, true); }

void disp_plot(int x, int y) {
	SetPixel(buf, x, y, true);
	render(buf, &frame_area);
}

void disp_line0(int x1, int y1, int x2, int y2) {
	DrawLine(buf, x1, y1, x2, y2, true);
}

void disp_line(int x1, int y1, int x2, int y2) {
	DrawLine(buf, x1, y1, x2, y2, true);
	render(buf, &frame_area);
}

void render_display() { render(buf, &frame_area); }

void display_ram_viewer() {

	for (int line = 0; line < LINES; line++) {

		// int offset = tbmon_idx + (line * BYTES_PER_ROW);
		int offset = tbmon_idx + (line * BYTES_PER_ROW);
		uint8_t byte_data[1];

		sprintf(tbmon_text_buffer[line], "%04x ", offset);

		for (int col = 0; col < BYTES_PER_ROW; col++) {

			if (col < 3) {
				sprintf(byte_data, "%02x:", sdram[offset + col]);
			} else {
				sprintf(byte_data, "%02x", sdram[offset + col]);
			}

			strcat(tbmon_text_buffer[line], byte_data);
		}

		strcat(tbmon_text_buffer[line], '\0');

		print_string(0, line, tbmon_text_buffer[line]);
	}
}

//
// UI Buttons
//

typedef enum { NONE, UP, DOWN, BACK, OK, CANCEL, CANCEL2 } button_state;

button_state read_button_state(void) {

	adc_select_input(2);
	uint16_t adc = adc_read();

	// print_string(0,3,"                   ", adc);

	// print_string(8,3,"ADC:%03x           ", adc);

	if (adc <= UP_ADC) { // UP 0x001
		return UP;
	} else if (adc <= DOWN_ADC) { // DOWN 0x210
		return DOWN;
	} else if (adc <= BACK_ADC) { // BACK 0x510
		return BACK;
	} else if (adc <= OK_ADC) { // OK 0x7B0
		return OK;
	} else if (adc <= CANCEL_ADC) { // CANCEL 0x8E0
		return CANCEL;
//	} else if (adc <= CANCEL2_ADC) { // CANCEL2 0xFFF
//		return CANCEL2;
	} else {
		return NONE;
	}
}

bool wait_for_button_release(void) {
	uint64_t last = time_us_64();
	while (read_button_state() != NONE) {
	}
	return (time_us_64() - last > LONG_BUTTON_DELAY);
}

void wait_for_button(void) {
	while (read_button_state() == NONE) {
	}
	wait_for_button_release();
	return;
}

bool wait_for_yes_no_button(void) {

	button_state button;
	while (true) {
		button = read_button_state();
		if (button == OK) {
			wait_for_button_release();
			return true;
		} else if (button == CANCEL || button == CANCEL2) {
			wait_for_button_release();
			return false;
		}
	}
}

//
// UI Display Loop
//

typedef enum { OFF, ON } disp_mode;

void display_loop() {

	disp_mode cur_disp_mode = ON; // DEFAULT is ON
	button_state buttons = NONE;

	//
	//
	//

	if (DEBUG_ADC) {

		reset_hold();
		clear_screen();
		uint16_t adc = adc_read();

		while (true) {
			
			adc_select_input(2);
			print_string(0, 1, "ADC:%03x       ", adc_read());
			sleep_ms(10);
		}
	}

	//
	//
	//

	while (true) {

		//
		//
		//

		if (cur_disp_mode != OFF) {

			sprintf(text_buffer, "%1x:%04x O:%02x I:%02x", cur_bank, d_adr,
					dr_op, dw_op);
			WriteString(buf, 0, 0, text_buffer);

			render(buf, &frame_area);
		}
		if ((cur_disp_mode == OFF) & (tbmon == false)) {

			print_string(0, 0, "L:%04x", low_adr);
			print_string(0, 1, "H:%04x", high_adr);
			print_string(0, 2, "&:%04x", m_adr);
			print_string(0, 3, "R:%02x", r_op);
			print_string(8, 3, "W:%02x", w_op);
		}

		//
		//
		//

		buttons = read_button_state();

		if (buttons != NONE) {

			reset_hold();
			confirmed = false;
			disabled = true;

			// in case it is in the 2nd part of the busy loop...
			read = true;
			written = true;

			while (!confirmed) {
			};

			// In your switch case:
			switch (buttons) {
			case UP:
				if (!tbmon_loaded) {
					pgm1();
					sleep_ms(DISPLAY_DELAY);
					sleep_ms(DISPLAY_DELAY);
				}
				if (tbmon) {
					if (tbmon_loaded) {
						// tbmon_idx += BYTES_PER_LINE;  // Move down by one
						// full line
						tbmon_idx -=
							BYTES_PER_ROW; // Move down by one full line
						if (tbmon_idx >= RAM_SIZE - (BYTES_PER_LINE * LINES)) {
							tbmon_idx = 0; // Wrap around if needed
						}
						// sleep_ms(DISPLAY_DELAY_SHORT);
					}
					tbmon_loaded = true;
					display_ram_viewer();
				} else {
					if (cur_disp_mode == ON)
						show_info();
					else
						clear_screen();
				}
				break;

			case DOWN:
				if (!tbmon_loaded) {
					pgm2();
					sleep_ms(DISPLAY_DELAY);
					sleep_ms(DISPLAY_DELAY);
				}
				if (tbmon) {
					if (tbmon_loaded) {
						// tbmon_idx -= BYTES_PER_LINE;  // Move up by one full
						// line
						tbmon_idx += BYTES_PER_ROW; // Move up by one full line
						if (tbmon_idx < 0) {
							tbmon_idx = RAM_SIZE -
										(BYTES_PER_LINE * LINES); // Wrap around
						}

						// sleep_ms(DISPLAY_DELAY_SHORT);
						// sleep_ms(DISPLAY_DELAY_SHORT);
					}
					tbmon_loaded = true;
					display_ram_viewer();
				} else {
					if (cur_disp_mode == ON)
						show_info();
					else
						clear_screen();
				}
				break;

			case BACK:
				// CHANGE CUR BANK

				cur_bank = (cur_bank + 1) % (MAX_BANKS);

				clear_screen();
				sprintf(text_buffer, "BANK #%1x", cur_bank);
				WriteString(buf, 0, 0, text_buffer);
				render(buf, &frame_area);
				sleep_ms(DISPLAY_DELAY);
				sleep_ms(DISPLAY_DELAY);
				if (cur_disp_mode == ON)
					show_info();
				else
					clear_screen();

				break;

			case OK:

				clear_screen();

				if (cur_disp_mode == OFF) {
					tbmon = true;
					wait_for_button_release();
					print_string(0, 0, "*    TB-MON    *");
					print_string(0, 1, "*     v 0.1    *");
					print_string(0, 2, "*    TurBoos   *");
					print_string(0, 3, "*     2025     *");

				}

				else {

					sprintf(text_buffer, "CLEAR BANK #%1x?", cur_bank);
					WriteString(buf, 0, 0, text_buffer);
					render(buf, &frame_area);
					wait_for_button_release();

					if (wait_for_yes_no_button()) {
						clear_bank(cur_bank);
						print_string(0, 3, "CLEARED!");
					} else
						print_string(0, 3, "CANCELED!");

					sleep_ms(DISPLAY_DELAY);
					sleep_ms(DISPLAY_DELAY);
					if (cur_disp_mode == ON)
						show_info();
					else
						clear_screen();
				}
				break;

			case CANCEL:
				if (tbmon) {
					tbmon = false;
					tbmon_loaded = false;
					tbmon_idx = 0;
				}
				if (cur_disp_mode == OFF)

					cur_disp_mode = ON;
				else
					cur_disp_mode = OFF;

				sleep_ms(DISPLAY_DELAY);
				sleep_ms(DISPLAY_DELAY);

				if (cur_disp_mode == ON)
					show_info();
				else
					clear_screen();

				break;
			}

			// gpio_put(LED_PIN, 0);
			disabled = false;
			reset_release();
		}
	}
}

int sd_read_init() {

	FRESULT fr;
	FATFS fs;
	FIL fil;

	int ret;
	char filename[] = "Z80NEO.INI";
	char buf[FILE_BUFF_SIZE];
	bool skip = false;

	clear_screen();

	// Initialize SD card
	if (!spi_configured) {
		print_string(0, 0, "INI - INIT");
		sleep_ms(DISPLAY_DELAY_LONG);
		skip = true;
	}

	// Mount drive
	if (!skip) {

		fr = f_mount(&fs, "", 1);

		if (FR_OK != fr) {
			print_string(0, 0, "INI - MOUNT");
			sleep_ms(DISPLAY_DELAY_LONG);
			skip = true;
		}
	}

	// Open file for reading
	if (!skip) {
		fr = f_open(&fil, filename, FA_READ);
		if (fr != FR_OK) {
			print_string(0, 0, "INI - OPEN");
			skip = true;
		}
	}

	// Z80.INI:
	// Z80))
	// F00
	// D80
	// C00
	// 800
	// 500
	// 200

	// COUNTER1.HEX
	// COUNTER2.HEX
	// COUNTER3.HEX
	// COUNTER4.HEX
	// 0

	while (!skip) {

		if (!f_gets(MACHINE, sizeof(MACHINE), &fil)) {
			show_error(0, 0, "INI - MACHINE");
			skip = true;
			break;
		}
		print_line(0, MACHINE);
		sleep_ms(DISPLAY_DELAY_SHORT);

		//
		//
		//

		if (!f_gets(buf, sizeof(buf), &fil)) {
			show_error(0, 0, "INI - CANCEL2");
			skip = true;
			break;
		}
		CANCEL2_ADC = decode_hex(buf[0]) * 16 * 16 + decode_hex(buf[1]) * 16 +
					  decode_hex(buf[2]);
		print_line(0, "%CANCEL2: %03x     ", CANCEL2_ADC);
		sleep_ms(DISPLAY_DELAY_SHORT);

		if (!f_gets(buf, sizeof(buf), &fil)) {
			show_error(0, 0, "INI - CANCEL");
			skip = true;
			break;
		}
		CANCEL_ADC = decode_hex(buf[0]) * 16 * 16 + decode_hex(buf[1]) * 16 +
					 decode_hex(buf[2]);
		print_line(0, "%CANCEL : %03x     ", CANCEL_ADC);
		sleep_ms(DISPLAY_DELAY_SHORT);

		if (!f_gets(buf, sizeof(buf), &fil)) {
			show_error(0, 0, "INI - OK");
			skip = true;
			break;
		}
		OK_ADC = decode_hex(buf[0]) * 16 * 16 + decode_hex(buf[1]) * 16 +
				 decode_hex(buf[2]);
		print_line(0, "%OK     : %03x     ", OK_ADC);
		sleep_ms(DISPLAY_DELAY_SHORT);

		if (!f_gets(buf, sizeof(buf), &fil)) {
			show_error(0, 0, "INI - BACK");
			skip = true;
			break;
		}
		BACK_ADC = decode_hex(buf[0]) * 16 * 16 + decode_hex(buf[1]) * 16 +
				   decode_hex(buf[2]);
		print_line(0, "%BACK   : %03x     ", BACK_ADC);
		sleep_ms(DISPLAY_DELAY_SHORT);

		if (!f_gets(buf, sizeof(buf), &fil)) {
			show_error(0, 0, "INI - DOWN");
			skip = true;
			break;
		}
		DOWN_ADC = decode_hex(buf[0]) * 16 * 16 + decode_hex(buf[1]) * 16 +
				   decode_hex(buf[2]);
		print_line(0, "%DOWN   : %03x     ", DOWN_ADC);
		sleep_ms(DISPLAY_DELAY_SHORT);

		if (!f_gets(buf, sizeof(buf), &fil)) {
			show_error(0, 0, "INI - UP");
			skip = true;
			break;
		}
		UP_ADC = decode_hex(buf[0]) * 16 * 16 + decode_hex(buf[1]) * 16 +
				 decode_hex(buf[2]);
		print_line(0, "%UP     : %03x     ", UP_ADC);
		sleep_ms(DISPLAY_DELAY_SHORT);

		//
		//
		//

		if (!f_gets(BANK_PROG[0], sizeof(BANK_PROG[0]), &fil)) {
			show_error(0, 0, "INI - PROG1");
			skip = true;
			break;
		}
		print_line(0, "P1: %12s", BANK_PROG[0]);
		sleep_ms(DISPLAY_DELAY_SHORT);

		if (!f_gets(BANK_PROG[1], sizeof(BANK_PROG[1]), &fil)) {
			show_error(0, 0, "INI - PROG2");
			skip = true;
			break;
		}
		print_line(0, "P2: %12s", BANK_PROG[1]);
		sleep_ms(DISPLAY_DELAY_SHORT);

		if (!f_gets(BANK_PROG[2], sizeof(BANK_PROG[2]), &fil)) {
			show_error(0, 0, "INI - PROG3");
			skip = true;
			break;
		}
		print_line(0, "P3:%12s", BANK_PROG[2]);
		sleep_ms(DISPLAY_DELAY_SHORT);

		if (!f_gets(BANK_PROG[3], sizeof(BANK_PROG[3]), &fil)) {
			show_error(0, 0, "INI - PROG 4");
			skip = true;
			break;
		}
		print_line(0, "P4: %12s", BANK_PROG[3]);
		sleep_ms(DISPLAY_DELAY_SHORT);

		break;
	}

	//
	//
	//

	if (!skip && f_gets(buf, sizeof(buf), &fil)) {
		DEBUG_ADC = buf[0] == '1';
		print_line(0, "%ADC DEBUG: 01x    ", DEBUG_ADC);
		sleep_ms(DISPLAY_DELAY_SHORT);

		clear_screen();
	}

	//
	//
	//

	// Close file
	fr = f_close(&fil);
	if (fr != FR_OK) {
		show_error(0, 0, "INI - CLOSE");
	}

	// Unmount drive
	f_unmount("0:");
}

//
//
//

static FRESULT fr;
static FATFS fs;
static FIL fil;
static char cwdbuf[FF_LFN_BUF] = {{0}};

void show_error_and_halt(char *err) {
	clear_screen();
	print_string(0, 0, err);
	while (true)
		;
}

void show_error(int b, int a, char *err) {

	clear_screen();
	print_string(a, b, err);
	sleep_ms(DISPLAY_DELAY_LONG);
	return;
}

void show_error_wait_for_button(char *err) {
	show_error(0, 0, err);
	wait_for_button_release();
	return;
}

char *init_and_mount_sd_card(void) {

	char fr_buf[10];

	memset(&cwdbuf, 0, FF_LFN_BUF);

	if (!spi_configured) {

		show_error_and_halt("SD INIT ERR1");
	}
	// Mount drive
	fr = f_mount(&fs, "", 1);

	if (fr != FR_OK) {
		sprintf(fr_buf, "SD INIT ERR2 %d", fr);
		show_error_and_halt(fr_buf);
		// show_error_and_halt("SD INIT ERR2");
	}
	fr = f_getcwd(cwdbuf, sizeof cwdbuf);
	if (FR_OK != fr) {
		show_error_and_halt("SD INIT ERR3");
	}

	return cwdbuf;
}

int count_files() {

	int count = 0;

	char const *p_dir;

	p_dir = init_and_mount_sd_card();

	DIR dj;		 /* Directory object */
	FILINFO fno; /* File information */
	memset(&dj, 0, sizeof dj);
	memset(&fno, 0, sizeof fno);

	fr = f_findfirst(&dj, &fno, p_dir, FILE_EXT);
	if (FR_OK != fr) {
		show_error(0, 0, "Count Files ERR");
		return 0;
	}

	while (fr == FR_OK && fno.fname[0]) {
		if (fno.fattrib & AM_DIR) {
			// directory
		} else {
			count++;
		}

		fr = f_findnext(&dj, &fno); /* Search for next item */
	}

	f_closedir(&dj);

	return count;
}

void clear_file_buffer() {

	for (int i = 0; i <= 16; i++)
		file[i] = 0;
}

int select_file_no(int no) {
	int count = 0;

	clear_file_buffer();

	char const *p_dir;

	p_dir = init_and_mount_sd_card();

	DIR dj;		 /* Directory object */
	FILINFO fno; /* File information */
	memset(&dj, 0, sizeof dj);
	memset(&fno, 0, sizeof fno);

	fr = f_findfirst(&dj, &fno, p_dir, FILE_EXT);
	if (FR_OK != fr) {
		show_error(0, 0, "File Sel ERR");
		return 0;
	}

	while (fr == FR_OK && fno.fname[0]) {
		if (fno.fattrib & AM_DIR) {
			// directory
		} else {
			count++;
			if (count == no) {
				// copy name into file buffer for display
				strcpy(file, fno.fname);
				return count;
			}
		}

		fr = f_findnext(&dj, &fno); /* Search for next item */
	}

	f_closedir(&dj);

	return 0;
}

int select_file() {

	clear_screen();
	int no = 1;
	int count = count_files();
	select_file_no(no);

	uint64_t last = time_us_64();

	bool blink = false;

	wait_for_button_release();

	while (read_button_state() != OK) {

		print_string(0, 0, "Load %2d of %2d", no, count);
		print_string(0, 3, "CANCEL OR OK");

		if (time_us_64() - last > BLINK_DELAY) {

			last = time_us_64();
			blink = !blink;

			if (blink)
				print_string(0, 1, "                ");
			else
				print_string(0, 1, file);
		}

		switch (read_button_state()) {
		case UP:
			wait_for_button_release();
			if (no < count)
				no = select_file_no(no + 1);
			else
				no = select_file_no(1);
			break;
		case DOWN:
			wait_for_button_release();
			if (no > 1)
				no = select_file_no(no - 1);
			else
				no = select_file_no(count);
			break;
		case CANCEL:
		case CANCEL2:
			wait_for_button_release();
			return -1;
		default:
			break;
		}
	}

	wait_for_button_release();

	return 0;
}

int create_name() {

	clear_screen();

	wait_for_button_release();

	clear_file_buffer();

	file[0] = 'A';
	file[1] = 0;

	int cursor = 0;

	uint64_t last = time_us_64();

	bool blink = false;

	read_button_state();

	while (read_button_state() != OK) {

		print_string(0, 0, "Save HEX - Name:");
		print_string(0, 3, "CANCEL OR OK");

		if (time_us_64() - last > BLINK_DELAY) {

			last = time_us_64();
			print_string0(0, 1, file);
			blink = !blink;

			if (blink)
				print_char(cursor, 1, '_');
			else
				print_char(cursor, 1, file[cursor]);
		}

		switch (read_button_state()) {
		case UP:
			if (file[cursor] < 127) {
				file[cursor]++;
				wait_for_button_release();
			}
			break;
		case DOWN:
			if (file[cursor] > 32) {
				file[cursor]--;
				wait_for_button_release();
			}
			break;
		case BACK:
			if (!wait_for_button_release()) { // short press?
				if (cursor < 7) {
					cursor++;
					if (!file[cursor])
						file[cursor] = file[cursor - 1];
				}
			} else {
				if (cursor > 0) {
					cursor--;
				}
			}
			break;
		case CANCEL:
		case CANCEL2:
			wait_for_button_release();
			return -1;
		default:
			break;
		}
	}

	cursor++;
	file[cursor++] = '.';
	file[cursor++] = 'H';
	file[cursor++] = 'E';
	file[cursor++] = 'X';
	file[cursor++] = 0;

	clear_screen();

	wait_for_button_release();

	return 0;
}

//
// PGM 1 - Load from SD Card
//

void load_file(bool quiet) {

	FRESULT fr;
	FATFS fs;
	FIL fil;
	int ret;
	char buf[FILE_BUFF_SIZE];
	char const *p_dir;

	if (!quiet) {
		clear_screen();
		print_string(0, 0, "Loading");
		print_string(0, 1, file);
		sleep_ms(DISPLAY_DELAY_SHORT);
	}

	p_dir = init_and_mount_sd_card();

	fr = f_open(&fil, file, FA_READ);

	if (fr != FR_OK) {
		sleep_ms(DISPLAY_DELAY_LONG);
		show_error(0, 0, "Can't open file!");
		sleep_ms(DISPLAY_DELAY_LONG);
		show_error(0, 0, file);
		sleep_ms(DISPLAY_DELAY_LONG);
		return;
	}

	bool readingComment = false;
	bool readingOrigin = false;

	//
	//
	//

	uint32_t pc = 0;
	byte val = 0;
	int count = 0;
	int line = 0;

	//
	//
	//
	
	if (DEBUG_LOAD){		
		print_string(0, 0, "OFFSET:");
		print_string(0, 2, "DATA:  ");
	}
	
	while (true) {

		memset(&buf, 0, sizeof(buf));
		if (!f_gets(buf, sizeof(buf), &fil))
			break;

		//line++;

		int i = 0;

		while (true) {

			byte b = buf[i++];

			if (!b)
				break;

			if (b == '\n' || b == '\r') {
				readingComment = false;
				readingOrigin = false;
			} else if (b == '#') {
				readingComment = true;
			} else if (b == '@') {
				readingOrigin = true;
			} else if (b == ':') {
				readingOrigin = true;
			}

			if (!readingComment && b != '\r' && b != '\n' && b != '\t' &&
				b != ' ' && b != '@' && b != ':') {

				int decoded = decode_hex(b);

				if (decoded == -1) {

					sprintf(text_buffer, "ERR LINE %05d", line);
					show_error_wait_for_button(text_buffer);
					clear_screen();
					fr = f_close(&fil);
					return;
				}
				
				
				if (!readingOrigin) {	
					
					
					if (DEBUG_LOAD){
						print_string(8, 3, "  ");
					}
					
					switch (count) {
						
						case 0:
							val = decoded * 16;
						
							if (DEBUG_LOAD){
								sprintf(text_buffer, "%02x", val);
								print_string(8, 3, text_buffer);
							}
							
							count = 1;
							break;
						case 1:
							val += decoded;
						
						
							if (DEBUG_LOAD){
								sprintf(text_buffer, "%02x", val);
								print_string(8, 3, text_buffer);
							}
							count = 0;
							sdram[pc++] = val;
							break;
					}
					
				} else {
					if (DEBUG_LOAD){
					    print_string(8, 3, "        ");
					}
					
					switch (count) {
						
						case 0:
							pc = decoded * 16 * 16 * 16;
						
							if (DEBUG_LOAD){
						
								sprintf(text_buffer, "%8x", pc);
								print_string(6, 1, text_buffer);
							}
							count = 1;
							break;
						case 1:
							pc += decoded * 16 * 16;
						
							if (DEBUG_LOAD){
						
								sprintf(text_buffer, "%8x", pc);
								print_string(6, 1, text_buffer);
							}
							count = 2;
							break;
						case 2:
							pc += decoded * 16;
						
							if (DEBUG_LOAD){
						
								sprintf(text_buffer, "%8x", pc);
								print_string(6, 1, text_buffer);
							}
							count = 3;
							break;
						case 3:
							pc += decoded;
						
							if (DEBUG_LOAD){
								sprintf(text_buffer, "%8x", pc);
								print_string(6, 1, text_buffer);
							}
							count = 0;
							readingOrigin = false;
							
							pc -= 0x1800;
							break;
						default:
							break;
					}
				}
				if (DEBUG_LOAD){
			 		sleep_ms(10);
				}
			    
			}
		}
		line++;
	}

	//
	//
	//

	fr = f_close(&fil);
	if (fr != FR_OK) {
		show_error(0, 0, "Cant't close file!");
	}

	// Unmount drive
	f_unmount("0:");

	//
	//
	//

	
	if (DEBUG_LOAD){
 		sleep_ms(100);
	}
    
	if (!quiet) {
		clear_screen();
		print_string(0, 0, "Loaded: RESET!");
		print_string(0, 1, file);
		sleep_ms(DISPLAY_DELAY);
	}

	strcpy(BANK_PROG[cur_bank], file);

	//
	//
	//

	for (uint32_t b = 0; b < RAM_SIZE; b++) {
//		uint32_t i = ((b & 0b00000000000000000000000000100000) ? 1 : 0) << 0x5 |
//					 ((b & 0b00000000000000000000000001000000) ? 1 : 0) << 0x0 |
//					 ((b & 0b00000000000000000000000010000000) ? 1 : 0) << 0x1 |
//					 ((b & 0b00000000000000000000000100000000) ? 1 : 0) << 0x2 |
//					 ((b & 0b00000000000000000000001000000000) ? 1 : 0) << 0x3 |
//					 ((b & 0b00000000000000000000010000000000) ? 1 : 0) << 0x4 |
//					 ((b & 0b00000000000000000000000000000001) ? 1 : 0) << 0x6 |
//					 ((b & 0b00000000000000000000000000000010) ? 1 : 0) << 0x7 |
//					 ((b & 0b00000000000000000000000000000100) ? 1 : 0) << 0x8 |
//					 ((b & 0b00000000000000000000000000001000) ? 1 : 0) << 0x9 |
//					 ((b & 0b00000000000000000000000000010000) ? 1 : 0) << 0xA;
		ram[cur_bank][b] = sdram[b];
	}

	//
	//
	//

	return;
}

void pgm1() {

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

//
//
//

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

	cur_bank = 4;
	if (BANK_PROG[4][0] >= 48) {
		print_string(0, 3, "LOAD PROG 4");
		sleep_ms(DISPLAY_DELAY);
		sleep_ms(DISPLAY_DELAY);
		strcpy(file, BANK_PROG[4]);
		load_file(true);
	}

	cur_bank = 5;
	if (BANK_PROG[5][0] >= 48) {
		print_string(0, 3, "LOAD PROG 5");
		sleep_ms(DISPLAY_DELAY);
		sleep_ms(DISPLAY_DELAY);
		strcpy(file, BANK_PROG[5]);
		load_file(true);
	}

	cur_bank = 6;
	if (BANK_PROG[6][0] >= 48) {
		print_string(0, 3, "LOAD PROG 6");
		sleep_ms(DISPLAY_DELAY);
		sleep_ms(DISPLAY_DELAY);
		strcpy(file, BANK_PROG[6]);
		load_file(true);
	}

	cur_bank = 7;
	if (BANK_PROG[7][0] >= 48) {
		print_string(0, 3, "LOAD PROG 7");
		sleep_ms(DISPLAY_DELAY);
		sleep_ms(DISPLAY_DELAY);
		strcpy(file, BANK_PROG[7]);
		load_file(true);
	}

	cur_bank = 0;
}

//
// PGM 2 - Save to SD Card
//

void pgm2() {

	// 0800
	// 000 - 7FFF

	for (uint32_t b = 0; b < RAM_SIZE; b++) {
		uint32_t i = ((b & 0b00000000000000000000000000100000) ? 1 : 0) << 0x5 |
					 ((b & 0b00000000000000000000000001000000) ? 1 : 0) << 0x0 |
					 ((b & 0b00000000000000000000000010000000) ? 1 : 0) << 0x1 |
					 ((b & 0b00000000000000000000000100000000) ? 1 : 0) << 0x2 |
					 ((b & 0b00000000000000000000001000000000) ? 1 : 0) << 0x3 |
					 ((b & 0b00000000000000000000010000000000) ? 1 : 0) << 0x4 |
					 ((b & 0b00000000000000000000000000000001) ? 1 : 0) << 0x6 |
					 ((b & 0b00000000000000000000000000000010) ? 1 : 0) << 0x7 |
					 ((b & 0b00000000000000000000000000000100) ? 1 : 0) << 0x8 |
					 ((b & 0b00000000000000000000000000001000) ? 1 : 0) << 0x9 |
					 ((b & 0b00000000000000000000000000010000) ? 1 : 0) << 0xA;
		sdram[b] = ram[cur_bank][i];
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

	FRESULT fr;
	FATFS fs;
	FIL fil;
	int ret;
	char buf[100];
	char const *p_dir;

	p_dir = init_and_mount_sd_card();

	fr = f_open(&fil, file, FA_READ);
	if (FR_OK == fr) {
		print_string(0, 2, "Overwrite File?");
		if (!wait_for_yes_no_button()) {
			print_string(0, 3, "*** CANCELED ***");
			sleep_ms(DISPLAY_DELAY);
			sleep_ms(DISPLAY_DELAY);
			f_close(&fil);
			return;
		} else
			print_string(0, 2, "FILE Open      ");
		f_close(&fil);
	}

	//
	//
	//

	// Open file for writing ()
	fr = f_open(&fil, file, FA_WRITE | FA_CREATE_ALWAYS);
	if (fr != FR_OK) {
		show_error(0, 0, "WRITE ERROR 1");
		f_close(&fil);
		return;
	}

	//
	//
	//

	byte val = 0;

	for (uint32_t pc = 0; pc < RAM_SIZE; pc++) {
		val = sdram[pc];
		ret = f_printf(&fil, (pc % 16 == 0) ? "\n%02X" : " %02X", val);
		if (ret < 0) {
			show_error(0, 0, "WRITE ERROR 2");
			f_close(&fil);
			return;
		}
	}

	//
	//
	//

	fr = f_close(&fil);
	
	if (fr != FR_OK) {
		show_error_wait_for_button("CANT'T CLOSE FILE");
		clear_screen();
	} else {
		strcpy(BANK_PROG[cur_bank], file);
		print_string(0, 3, "Saved: %s", file);
		sleep_ms(DISPLAY_DELAY);
		sleep_ms(DISPLAY_DELAY);
	}

	//
	//
	//

	return;
}

//
//
//

int center_string(char *string) {
	int n = strlen(string);
	int l = 8 - (n / 2);
	return l < 0 ? 0 : l;
}

void show_logo(void) {

	clear_screen();

	print_string(0, 0, "    TurBoRAM    ");
	print_string(0, 1, VERSION);
	print_string(center_string(MACHINE), 2, MACHINE);
	print_string(0, 3, " TurBoss  2025 ");
}

void show_info(void) {

	clear_screen();

	print_string(0, 0, "%s", BANK_PROG[cur_bank]);
	print_string(0, 1, "BANK:%d SIZE:%04x", MAX_BANKS, RAM_SIZE);
	print_string(0, 2, "TYPE:%s", MACHINE);
	print_string(0, 3, "BANK:%d", cur_bank);
}

//
//
//

void reset_release(void) {
	// gpio_set_dir(RESET_OUT, GPIO_IN);
}

void reset_hold(void) {
	//	gpio_set_dir(RESET_OUT, GPIO_OUT);
	//	gpio_put(RESET_OUT, 0);
}

//
//
//

// 0 IN, 1 OUT
void set_bus_dir(int direction) {
	
	// BUS GPIO 0 <--> 
	int pin = 0;
	if (direction) {
		for (pin = BUS_GPIO_START; pin < BUS_GPIO_END; pin++) {
			gpio_set_dir(pin, GPIO_OUT);
		}
	}
	else {
		for (pin = BUS_GPIO_START; pin < BUS_GPIO_END; pin++) {
			gpio_set_dir(pin, GPIO_IN);
		}
	}
}


//


/*
void nop_delay(){
	 __asm volatile (" nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n  nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n  nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n");
	 }
*/


bool mreq = true;
bool rd = true;

bool iorq = true;
bool wr = true;


uint8_t r_delay = 3;
uint8_t rd_delay = 3;
uint8_t w_delay = 3;


void bus_callback(uint pin, uint32_t events) {


	if (pin == IORQ_INPUT) {


		// DIRECTION 1 ON LOW ADDRESS
		gpio_put(DIR1_OUT, 0);
		gpio_put(DIR2_OUT, 1);
		gpio_put(DIR3_OUT, 1);

		// SELECT 1 ON LOW ADDRESS
		gpio_put(SEL1_OUT, 0);
		gpio_put(SEL2_OUT, 1);
		gpio_put(SEL3_OUT, 1);

		sleep_ms(r_delay);
		
		set_bus_dir(0);
		
		low_adr = (((gpio_get_all() & bus_mask) >> BUS_GPIO_START) & 0b11111111); // A0 - A7


		// DIRECTION 2 ON HIGH ADDRESS
		gpio_put(DIR1_OUT, 1);
		gpio_put(DIR2_OUT, 0);
		gpio_put(DIR3_OUT, 1);
		
		// SELECT 2 ON HIGH ADDRESS
		gpio_put(SEL1_OUT, 1);
		gpio_put(SEL2_OUT, 0);
		gpio_put(SEL3_OUT, 1);

		sleep_ms(r_delay);
		
		set_bus_dir(0);
		
		high_adr = (((gpio_get_all() & bus_mask) >> BUS_GPIO_START) & 0b11111111)	<< 8; // A8 - A15
		// 	high_adr =  0x00;
		
		// m_adr = 0x00;
		m_adr = low_adr | high_adr;
		
		wr = gpio_get(IORQ_INPUT);
		

		// MEMORY WRITE
		if (!wr) {

			// sleep_ms(0);
			
			// SLECT 3 DATA
			gpio_put(SEL1_OUT, 1);
			gpio_put(SEL2_OUT, 1);
			gpio_put(SEL3_OUT, 0);

			// DIRECTION 3 DATA
			gpio_put(DIR1_OUT, 1);
			gpio_put(DIR2_OUT, 1);
			gpio_put(DIR3_OUT, 0);

		
		    gpio_set_dir_masked(bus_mask, bus_mask);
		    
			sleep_ms(w_delay);
		
		
			set_bus_dir(0);
			
			r_op = (gpio_get_all() & bus_mask) >> BUS_GPIO_START ;


			if (low_adr == 0x40){
				printf("%c", r_op);
			}
			else{
				ram[cur_bank][m_adr] = r_op;
			}
	
			sleep_ms(0);
		
			gpio_set_dir_masked(bus_mask, 0);

		}

		
		// DIRECTION OFF
		gpio_put(DIR1_OUT, 1);
		gpio_put(DIR2_OUT, 1);
		gpio_put(DIR3_OUT, 1);
		
		// DIRECTION OFF
		gpio_put(SEL1_OUT, 1);
		gpio_put(SEL2_OUT, 1);
		gpio_put(SEL3_OUT, 1);
		
			

	}

	else if (pin == MREQ_INPUT) {

		mreq = gpio_get(MREQ_INPUT);
		rd = gpio_get(RD_INPUT);
		
		// MEMORY READ
		if (!mreq && !rd) {


			// DIRECTION 1 ON LOW ADDRESS
			gpio_put(DIR1_OUT, 0);
			gpio_put(DIR2_OUT, 1);
			gpio_put(DIR3_OUT, 1);
	
			// SELECT 1 ON LOW ADDRESS
			gpio_put(SEL1_OUT, 0);
			gpio_put(SEL2_OUT, 1);
			gpio_put(SEL3_OUT, 1);
	
			sleep_ms(w_delay);
	
		
			set_bus_dir(0);
		
			low_adr = (((gpio_get_all() & bus_mask) >> BUS_GPIO_START) & 0b11111111); // A0 - A7
	
			// DIRECTION OFF
			gpio_put(DIR1_OUT, 1);
			gpio_put(DIR2_OUT, 1);
			gpio_put(DIR3_OUT, 1);
			
			// SELECT OFF
			gpio_put(SEL1_OUT, 1);
			gpio_put(SEL2_OUT, 1);
			gpio_put(SEL3_OUT, 1);
			

			gpio_set_dir_masked(bus_mask, 0);
			
			
			mreq = gpio_get(MREQ_INPUT);
			rd = gpio_get(RD_INPUT);
	
			// Check if NOT RD and NOT MREQ
			
			if (!mreq && !rd) {
			
				// DIRECTION 2 ON HIGH ADDRESS
				gpio_put(DIR1_OUT, 1);
				gpio_put(DIR2_OUT, 0);
				gpio_put(DIR3_OUT, 1);
				
				// SELECT 2 ON HIGH ADDRESS
				gpio_put(SEL1_OUT, 1);
				gpio_put(SEL2_OUT, 0);
				gpio_put(SEL3_OUT, 1);
				
				sleep_ms(w_delay);
				
		
				set_bus_dir(0);
		
				high_adr = (((gpio_get_all() & bus_mask) >> BUS_GPIO_START) & 0b11111111) << 8; // A8 - A15
	
	
				// DIRECTION OFF
				gpio_put(DIR1_OUT, 1);
				gpio_put(DIR2_OUT, 1);
				gpio_put(DIR3_OUT, 1);
				
				// SELECT OFF
				gpio_put(SEL1_OUT, 1);
				gpio_put(SEL2_OUT, 1);
				gpio_put(SEL3_OUT, 1);
				
	
				gpio_set_dir_masked(bus_mask, 0);
				
				m_adr = low_adr | high_adr;
				
				
		
				// DIRECTION 3 DATA
				gpio_put(DIR1_OUT, 1);
				gpio_put(DIR2_OUT, 1);
				gpio_put(DIR3_OUT, 1);
				
				// SLECT 3 DATA
				gpio_put(SEL1_OUT, 1);
				gpio_put(SEL2_OUT, 1);
				gpio_put(SEL3_OUT, 0);
			
				
				w_op = ram[cur_bank][m_adr];
				
		
				set_bus_dir(1);
				
				gpio_set_dir_masked(bus_mask, bus_mask);
		
				gpio_put_masked(bus_mask, (w_op << BUS_GPIO_START));


				sleep_ms(w_delay);
				
				// DIRECTION OFF
				gpio_put(DIR1_OUT, 1);
				gpio_put(DIR2_OUT, 1);
				gpio_put(DIR3_OUT, 1);
				
				// SELECT OFF
				gpio_put(SEL1_OUT, 1);
				gpio_put(SEL2_OUT, 1);
				gpio_put(SEL3_OUT, 1);
			
				
				gpio_put_masked(bus_mask, (0 << BUS_GPIO_START));
				gpio_set_dir_masked(bus_mask, 0);
			}
		}
	}
}

//
//
//


void custom_cdc_task(void)
{
    // polling CDC interfaces if wanted

    // Check if CDC interface 0 (for pico sdk stdio) is connected and ready

    if (tud_cdc_n_connected(0)) {
        // print on CDC 0 some debug message
        // printf("Connected to CDC 0\n");
        // sleep_ms(5000); // wait for 5 seconds
    }
}

// callback when data is received on a CDC interface
void tud_cdc_rx_cb(uint8_t itf)
{
	
    // allocate buffer for the data in the stack
    uint8_t buf[CFG_TUD_CDC_RX_BUFSIZE];


    // printf("RX CDC %d\n", itf);



    // read the available data 
    // | IMPORTANT: also do this for CDC0 because otherwise
    // | you won't be able to print anymore to CDC0
    // | next time this function is called
    uint32_t count = tud_cdc_n_read(itf, buf, sizeof(buf));

    // check if the data was received on the second cdc interface
    if (itf == 1) {
        // process the received data
        buf[count] = 0; // null-terminate the string
        // now echo data back to the console on CDC 0
        printf("Received on CDC 1: %s\n", buf);

        // and echo back OK on CDC 1
        tud_cdc_n_write(itf, (uint8_t const *) "OK\r\n", 4);
        tud_cdc_n_write_flush(itf);
    }
    else {
        // process the received data
        buf[count] = 0; // null-terminate the string
        // now echo data back to the console on CDC 0
        printf("Received on CDC 0: %s\n", buf);

        // and echo back OK on CDC 1
//        tud_cdc_n_write(itf, (uint8_t const *) "OK\r\n", 4);
//        tud_cdc_n_write_flush(itf);
	}
}

static uint8_t display_buf[SSD1306_BUF_LEN];

bool previous_ce = false;
bool previous_we = false;

int main() {

	// Set system clock speed.
	// 125 MHz

	// set_sys_clock_pll(1100000000, 4, 1);

	//
	//
	//
	
    board_init();
    tusb_init();

    // TinyUSB board init callback after init
    if (board_init_after_tusb) {
        board_init_after_tusb();
    }
    
	stdio_init_all();

	sleep_ms(100);

	// Tell GPIO 0 it is allocated to the PWM
	gpio_set_function(GPIO_PWM_SIG, GPIO_FUNC_PWM);

	// Find out which PWM slice is connected to GPIO 0 (it's slice 0)
	uint slice_num = pwm_gpio_to_slice_num(GPIO_PWM_SIG);

	// Set the PWM frequency to 50 Hz
	// The system clock is typically 125 MHz, so we need to divide it down
	// 125,000,000 Hz / 50 Hz = 2,500,000
	// We need to set the wrap value to 2,500,000 / 4096 (max duty cycle
	// resolution)
	// uint32_t clock_div = 2500000 / 4096; // 50hz
	
	uint32_t clock_div = (clock_get_hz(clk_sys)/50) / 4096;
	
	pwm_set_clkdiv(slice_num, clock_div);

	// Set period of 4096 cycles (for 12-bit resolution)
	pwm_set_wrap(slice_num, 4095);

	// Set the duty cycle
	pwm_set_chan_level(slice_num, PWM_CHAN_A, 2049); // between 0 and 4096

	

	pico_fatfs_spi_config_t fs_config = {
		spi0, // if unmatched SPI pin assignments with spi0/spi1 or explicitly
			  // designated as NULL, SPI PIO will be configured
		CLK_SLOW_DEFAULT, CLK_FAST_DEFAULT,
		PIN_SPI1_MISO,	 // SPIx_RX
		PIN_SPI1_SCK,	 // SPIx_CS
		PIN_SPI1_CS,	 // SPIx_SCK
		PIN_SPI1_MOSI,	 // SPIx_TX
		true // use internal pullup
	};

	spi_configured = pico_fatfs_set_config(&fs_config);

	gpio_init(LED_PIN);
	gpio_set_dir(LED_PIN, GPIO_OUT);

	//
	//
	//

	//	gpio_init(RESET_OUT);
	//	gpio_set_function(RESET_OUT, GPIO_FUNC_SIO);
	//
	//	reset_hold();

	//
	//
	//

	adc_init();
	adc_gpio_init(ADC_KEYS_INPUT);

	//
	//
	//

	ssd1306_setup();

	calc_render_area_buflen(&frame_area);
	// zero the entire display
	memset(display_buf, 0, SSD1306_BUF_LEN);
	render(display_buf, &frame_area);

	show_logo();

	sleep_ms(DISPLAY_DELAY_LONG);
	sleep_ms(DISPLAY_DELAY_LONG);

	//
	//
	//

	cur_bank = 0;

	for (uint8_t pgm = 0; pgm < MAX_BANKS; pgm++) {
		clear_bank(pgm);
	}

	//
	//
	//

	// sd_test();

	clear_screen();
	WriteString(buf, 0, 0, "SD READ");
	render(buf, &frame_area);
	sleep_ms(DISPLAY_DELAY_SHORT);

	sd_read_init();

	/*
	clear_screen();
	WriteString(buf, 0, 0, "LOAD PROGS");
	render(buf, &frame_area);
	sleep_ms(DISPLAY_DELAY_SHORT);


	load_init_progs();
	*/

	clear_screen();

	WriteString(buf, 0, 0, "SHOW INFO");
	render(buf, &frame_area);
	sleep_ms(DISPLAY_DELAY_SHORT);

	show_info();

	//
	// INIT GPIO
	//

	// BUS GPIO 0 <--> 8
	for (gpio = BUS_GPIO_START; gpio < BUS_GPIO_END; gpio++) {
		bus_mask |= (1 << gpio);
		gpio_init(gpio);
		gpio_set_function(gpio, GPIO_FUNC_SIO);
		gpio_set_dir(gpio, GPIO_IN);
	}

	// ADDRESS SELECT 0 <--> 7

	gpio_init(SEL1_OUT);
	gpio_set_function(SEL1_OUT, GPIO_FUNC_SIO);
	gpio_set_dir(SEL1_OUT, GPIO_OUT);
	gpio_set_outover(SEL1_OUT, GPIO_OVERRIDE_NORMAL);
	gpio_put(SEL1_OUT, 1);

	// ADDRESS SELECT 8 <--> 15

	gpio_init(SEL2_OUT);
	gpio_set_function(SEL2_OUT, GPIO_FUNC_SIO);
	gpio_set_dir(SEL2_OUT, GPIO_OUT);
	gpio_set_outover(SEL2_OUT, GPIO_OVERRIDE_NORMAL);
	gpio_put(SEL2_OUT, 1);

	// DATA SELECT 0 <--> 7

	gpio_init(SEL3_OUT);
	gpio_set_function(SEL3_OUT, GPIO_FUNC_SIO);
	gpio_set_dir(SEL3_OUT, GPIO_OUT);
	gpio_set_outover(SEL3_OUT, GPIO_OVERRIDE_NORMAL);
	gpio_put(SEL3_OUT, 1);

	// BUS DIRECTION

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
	
	// CPU RD

	gpio_init(RD_INPUT);
	gpio_set_function(RD_INPUT, GPIO_FUNC_SIO);
	gpio_set_dir(RD_INPUT, GPIO_IN);
	gpio_set_inover(RD_INPUT, GPIO_OVERRIDE_NORMAL);

	//
	//
	//

	m_adr = 0;
	r_op = 0;
	w_op = 0;

	multicore_launch_core1(display_loop);

	while (DEBUG_ADC) {
		sleep_ms(1000);
	}
	//
	//
	//

	read = false;
	written = false;
	confirmed = false;

	gpio_set_dir_masked(bus_mask, 0);

	gpio_put(SEL1_OUT, 1);
	gpio_put(SEL2_OUT, 1);
	gpio_put(SEL3_OUT, 1);

	reset_release();

	// gpio_set_irq_enabled_with_callback(21, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
	
	gpio_set_irq_enabled_with_callback(MREQ_INPUT, GPIO_IRQ_EDGE_FALL, true, &bus_callback);
	gpio_set_irq_enabled_with_callback(IORQ_INPUT, GPIO_IRQ_EDGE_FALL, true, &bus_callback);
									   
	//	gpio_set_irq_enabled(RD_INPUT, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
	// 	gpio_set_irq_enabled(WR_INPUT, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);

	// Set the PWM running
	pwm_set_enabled(slice_num, true);
	
	while (true) {

		if (disabled) {
			gpio_put(LED_PIN, 1);
			confirmed = true;
			while (disabled) {
			};
		}
		
		tud_task();
		
		// custom tasks
		custom_cdc_task();

		// ADDRESS

		//		do {  // 135 ns (11000.... high clock!)

		//		if (!gpio_get(MREQ_INPUT)) {
		//
		//			previous_ce = true;
		//
		//			gpio_put(SEL2_OUT, 1);
		//			gpio_put(SEL1_OUT, 0);
		//
		//
		//			sleep_ms(40);
		//
		//			low_adr = (((gpio_get_all() & addr_mask) >> BUS_GPIO_START )
		//& 0b11111111);  // A0 - A7
		//
		//			sleep_ms(40);
		//
		//			gpio_put(SEL1_OUT, 1);
		//			gpio_put(SEL2_OUT, 1);
		//
		//			sleep_ms(40);
		//
		//			gpio_put(SEL1_OUT, 1);
		//			gpio_put(SEL2_OUT, 0);
		//
		//			sleep_ms(40);
		//
		//			high_adr = (((gpio_get_all() & addr_mask) >> BUS_GPIO_START
		//) & 0b11111111) << 8;  // A8 - A15
		////			high_adr =  0x00;
		//
		//			sleep_ms(40);
		//
		//			gpio_put(SEL2_OUT, 1);
		//			gpio_put(SEL1_OUT, 1);
		//
		//
		////			m_adr = 0x00;
		// 			m_adr = low_adr | high_adr;
		//
		//
		//
		//			sleep_ms(40);
		////
		////////
		//		}
		//
		//		// WRTITE DATA TO RAM
		//
		//		if (gpio_get(RD_INPUT)) {
		//
		//			gpio_put(SEL3_OUT, 0);
		//			gpio_put(RW_OUT, 0);
		//			gpio_set_dir_masked(data_mask, 0);
		//
		//
		//			sleep_ms(40);
		//
		//			// wait until address is stable! ~80 ns or so...
		//			// __asm volatile (" nop\n nop\n nop\n nop\n nop\n nop\n
		// nop\n nop\n nop\n nop\n nop\n nop\n nop\n");
		//
		//			r_op = (gpio_get_all() & data_mask) >> BUS_GPIO_END;
		//
		//			ram[cur_bank][m_adr] = r_op;
		//
		//			read = true;
		//
		//			sleep_ms(40);
		//
		//			gpio_put(RW_OUT, 1);
		//			gpio_put(SEL3_OUT, 1);
		//
		//
		//		}
		//
		//
		//		// READ DATA FROM RAM
		//		if (gpio_get(WR_INPUT)) {
		//
		//			gpio_put(SEL3_OUT, 0);
		//			gpio_put(RW_OUT, 1);
		//
		//			sleep_ms(40);
		//
		//			w_op = ram[cur_bank][m_adr];
		//
		//			gpio_set_dir_masked(data_mask, data_mask);
		//			gpio_put_masked(data_mask, (w_op << BUS_GPIO_END));
		//
		//			written = true;
		//
		//			sleep_ms(40);
		//
		//			gpio_put(RW_OUT, 0);
		//			gpio_put(SEL3_OUT, 1);
		//
		//
		//
		//		}
		//
		//
		//
		//
		//
		//
		//
		//
		////////
		////
		//
		//
		//
		//
		//
		//
		//
		///*		}
		//		else if (! gpio_get(WR_INPUT) ) {
		//			previous_ce = false;
		//		}
		//		//} while (!gpio_get(MREQ_INPUT) && ! disabled );
		//*/
		////		}
		//
		//
		//		//
		//		//
		//		//
		//
		//
		//		// DATA
		//
		//
		//
		////		read = false;
		//// 		written = false;
		//
		////		if ( !gpio_get(WR_INPUT) && ! gpio_get(RD_INPUT) && ! read
		///&& ! written && ! disabled ) {
		//
		//		// READ DATA
		///*
		//		while (!gpio_get(MREQ_INPUT) && ! disabled) {
		//
		//
		//			if (!gpio_get(RD_INPUT) && ! gpio_get(WR_INPUT) ) {
		//
		//				gpio_put(SEL3_OUT, 0);
		//				gpio_put(RW_OUT, 1);
		//				gpio_set_dir_masked(data_mask, 0);
		//
		//
		//				sleep_ms(0);
		//
		//				// wait until address is stable! ~80 ns or so...
		//				// __asm volatile (" nop\n nop\n nop\n nop\n nop\n nop\n
		// nop\n nop\n nop\n nop\n nop\n nop\n nop\n");
		//
		//				r_op = (gpio_get_all() & data_mask) >> BUS_GPIO_START ;
		//
		//				ram[cur_bank][m_adr] = r_op;
		//
		//				read = true;
		//
		//				sleep_ms(0);
		//
		//				gpio_put(RW_OUT, 0);
		//				gpio_put(SEL3_OUT, 1);
		//
		//
		//			}
		//
		//
		//			// WRITTE DATA
		//			if ( gpio_get(RD_INPUT) && ! gpio_get(WR_INPUT) ) {
		//
		//				gpio_put(SEL3_OUT, 0);
		//				gpio_put(RW_OUT, 0);
		//
		//				sleep_ms(0);
		//
		//				w_op = ram[cur_bank][m_adr];
		//
		//				gpio_set_dir_masked(data_mask, data_mask);
		//				gpio_put_masked(data_mask, (w_op << BUS_GPIO_START));
		//
		//				written = true;
		//
		//				sleep_ms(0);
		//
		//				gpio_put(RW_OUT, 1);
		//				gpio_put(SEL3_OUT, 1);
		//
		//
		//
		//			}
		//
		// 		}
		//*/
		//
		//		gpio_set_dir_masked(data_mask, 0);
	}
}
