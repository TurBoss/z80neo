/*
 * uart1term.c — console <-> UART1 terminal bridge for the z80neo.
 *
 * Bridges the on-board console (port 0x80 data / 0x81 status, i.e. the VDP or
 * the PC sdl_client) to the second UART the firmware exposes at 0x90 data /
 * 0x91 status (hardware UART1 on GPIO36/37).  Anything typed on the console is
 * sent to UART1, and anything received on UART1 is printed to the console, so a
 * device on UART1 can be used interactively.
 *
 * Status bits are the same on both ports: bit0 = RX ready, bit1 = TX ready.
 * The banner shows the live UART1 line settings, read back from the firmware
 * config ports 0x92 (baud) / 0x93 (format).  Those defaults come from Z80NEO.INI
 * (UART1_BAUD / UART1_FORMAT); a CP/M program (TTERM) can also retune UART1 at
 * runtime through the same ports.
 *
 * Build with the in-tree z88dk target:
 *     zcc +z80neo uart1term.c -o uart1term -create-app     # -> uart1term.ihx
 *
 * Copy uart1term.ihx to the SD-card root and point Z80NEO.INI BANK_PROG[0] at
 * it.  Use a terminal with local echo OFF (the VDP and sdl_client already are).
 *
 * Hardware: jumper EXT2 pin 16 (GPIO36 = UART1 TX) to EXT2 pin 17 (GPIO37 =
 * UART1 RX) for a loopback self-test (every typed character echoes once), or
 * wire a 3.3 V device (GPIO36 -> its RX, GPIO37 <- its TX, common GND).
 */

#include <stdint.h>

__sfr __at 0x80 CONSOLE_DATA;
__sfr __at 0x81 CONSOLE_STATUS; // bit0 = RX ready, bit1 = TX ready
__sfr __at 0x90 UART1_DATA;
__sfr __at 0x91 UART1_STATUS;   // bit0 = RX ready, bit1 = TX ready
__sfr __at 0x92 UART1_CFG_BAUD; // read: baud code (0 = 115200)
__sfr __at 0x93 UART1_CFG_FMT;  // read: format byte (0 = 8N1)

static const char *const baud_names[] = {
	"115200", "57600", "38400", "19200", "9600", "4800", "2400", "1200", "600", "300", "31250"
};
#define BAUD_CODE_COUNT (sizeof(baud_names) / sizeof(baud_names[0]))

static void con_put(uint8_t c)
{
	while ((CONSOLE_STATUS & 0x02) == 0)
		;
	CONSOLE_DATA = c;
}

static void con_puts(const char *s)
{
	while (*s)
		con_put((uint8_t)*s++);
}

static void con_hex(uint8_t v)
{
	static const char hex[] = "0123456789ABCDEF";

	con_put((uint8_t)hex[v >> 4]);
	con_put((uint8_t)hex[v & 0x0F]);
}

static void con_settings(void)
{
	uint8_t fmt = UART1_CFG_FMT;
	uint8_t code = UART1_CFG_BAUD;
	uint8_t db, par, sb;

	if (!(fmt & 0x80))
		fmt = 0x03;             // no config port: hardware default is 8N1
	fmt &= 0x3F;
	db = 5 + (fmt & 0x03);
	par = (fmt >> 4) & 0x03;
	sb = (fmt & 0x08) ? 2 : 1;

	con_puts(code < BAUD_CODE_COUNT ? baud_names[code] : "?");
	con_put(' ');
	con_put((uint8_t)('0' + db));
	con_put((uint8_t)(par == 1 ? 'E' : par == 2 ? 'O' : 'N'));
	con_put((uint8_t)('0' + sb));
}

/* Returns 1 if the byte was accepted, 0 on timeout (TX never became ready). */
static int uart1_put(uint8_t c)
{
	unsigned n = 0xFFFF;

	while ((UART1_STATUS & 0x02) == 0)
		if (--n == 0)
			return 0;
	UART1_DATA = c;
	return 1;
}

int main(void)
{
	con_puts("\r\n"
	         "z80neo UART1 terminal bridge\r\n"
	         "console <-> UART1 (0x90/0x91, GPIO36/37)\r\n"
	         "UART1: ");
	con_settings();
	con_puts("   status: 0x");
	con_hex(UART1_STATUS);
	con_puts(" (want bit1=TX ready)\r\n\r\n");

	for (;;) {
		if (CONSOLE_STATUS & 0x01) { // host typed something
			if (!uart1_put(CONSOLE_DATA)) {
				con_puts("[uart1 tx timeout]\r\n");
				continue;
			}
		}
		if (UART1_STATUS & 0x01) // device sent something
			con_put(UART1_DATA);
	}

	return 0;
}

