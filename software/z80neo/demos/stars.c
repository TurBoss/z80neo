/*
 * stars.c - per-pixel-coloured starfield on the VoidVDP bitmap layer.
 *
 * Each of the 90 stars has its own palette colour and brightness, written one
 * pixel at a time with z80neo_gfx_pixel().  Press q or ESC to quit.
 *
 * (Per-pixel colour needs the RP2350 VDP; on an RP2040 VDP the layer is 1bpp
 * and the colours are approximated in pairs per character cell.)
 */

#include <stdint.h>
#include <z80neo.h>
#include "common.h"

#define NSTARS 90

static uint16_t sx[NSTARS], sy[NSTARS];
static int8_t vx[NSTARS], vy[NSTARS];
static uint8_t sc[NSTARS];

int main(void)
{
	static const uint8_t bright[8] = { 9, 10, 11, 12, 13, 14, 15, 7 };

	for (int i = 0; i < NSTARS; i++) {
		sx[i] = (uint16_t)(demo_rand() % 640u);
		sy[i] = (uint16_t)(demo_rand() % 256u);
		vx[i] = (int8_t)((int)(demo_rand() % 5u) - 2);
		vy[i] = (int8_t)(demo_rand() % 3u);
		sc[i] = bright[demo_rand() & 7];
	}

	z80neo_gfx_color(0, 0);       /* background is colour 0 (black) */
	z80neo_gfx_bitmap(1);

	for (;;) {
		z80neo_gfx_clg();

		for (int i = 0; i < NSTARS; i++) {
			z80neo_gfx_pixel(sx[i], sy[i], sc[i]);

			int nx = (int)sx[i] + vx[i];
			int ny = (int)sy[i] + vy[i];
			if (nx < 0)
				nx = 639;
			else if (nx > 639)
				nx = 0;
			if (ny < 0)
				ny = 255;
			else if (ny > 255)
				ny = 0;
			sx[i] = (uint16_t)nx;
			sy[i] = (uint16_t)ny;
		}

		if (demo_quit())
			break;
	}

	z80neo_gfx_bitmap(0);
	return 0;
}
