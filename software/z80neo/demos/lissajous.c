/*
 * lissajous.c - parametric curves on the VoidVDP bitmap layer.
 *
 * Each curve is drawn from 128 hardware line segments.  A new phase/colour
 * is drawn each time a key is pressed; q or ESC quits.
 */

#include <stdint.h>
#include <z80neo.h>
#include "common.h"

static void curve(uint8_t denom_a, uint8_t denom_b, uint8_t phase, uint8_t colour)
{
	int px = 0, py = 0;

	z80neo_gfx_color(colour, 0);
	for (int t = 0; t <= 255; t += 2) {
		uint8_t ta = (uint8_t)((t * denom_a) / 16u);
		uint8_t tb = (uint8_t)((t * denom_b) / 16u + phase);
		int x = 320 + (220 * demo_isin(ta)) / 256;
		int y = 128 + (100 * demo_isin(tb)) / 256;
		if (t != 0)
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
		curve(3, 2, phase, 11);
		curve(5, 4, (uint8_t)(phase + 40), 14);
		curve(7, 6, (uint8_t)(phase + 80), 10);

		for (;;) {
			int c = getk();
			if (c == 'q' || c == 'Q' || c == 27)
				goto done;
			if (c)
				break;
		}
		phase = (uint8_t)(phase + 5);
	}

done:
	z80neo_gfx_bitmap(0);
	return 0;
}
