/*
 * wave.c - animated sine waves on the VoidVDP bitmap layer.
 *
 * Two waves drawn as 63 hardware line segments each.  Press q or ESC to quit.
 */

#include <stdint.h>
#include <z80neo.h>
#include "common.h"

static void wave(uint8_t colour, uint8_t phase, int amp)
{
	int px = 0, py = 0;

	z80neo_gfx_color(colour, 0);
	for (int i = 0; i < 64; i++) {
		int x = i * 10;
		int y = 128 + (amp * demo_isin((uint8_t)(i * 4 + phase))) / 256;
		if (i)
			z80neo_gfx_line((uint16_t)px, (uint16_t)py, (uint16_t)x, (uint16_t)y, 1);
		px = x;
		py = y;
	}
}

int main(void)
{
	uint8_t phase = 0;

	z80neo_gfx_bitmap(1);

	for (;;) {
		z80neo_gfx_clg();
		wave(11, phase, 100);
		wave(14, (uint8_t)(phase * 2 + 64), 60);
		phase = (uint8_t)(phase + 3);
		if (demo_quit())
			break;
	}

	z80neo_gfx_bitmap(0);
	return 0;
}
