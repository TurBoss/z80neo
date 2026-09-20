/*
 * bars.c - animated colour bars on the VoidVDP bitmap layer.
 *
 * 80 solid bars, each filled with its own palette colour, cycling over time.
 * Uses z80neo_gfx_color() + z80neo_gfx_fill() (per-pixel colour in the
 * framebuffer).  Press q or ESC to quit.
 */

#include <stdint.h>
#include <z80neo.h>
#include "common.h"

#define NBARS 80
#define BARW 8

int main(void)
{
	uint8_t phase = 0;

	z80neo_gfx_bitmap(1);

	for (;;) {
		for (int i = 0; i < NBARS; i++) {
			uint8_t c = (uint8_t)((i + phase) & 15);
			z80neo_gfx_color(c, 0);
			z80neo_gfx_fill((uint16_t)(i * BARW), 0, BARW, 256, 1);
		}
		phase = (uint8_t)(phase + 1);
		if (demo_quit())
			break;
	}

	z80neo_gfx_bitmap(0);
	return 0;
}
