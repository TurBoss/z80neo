/*
 * vdpdemo.c - z80neo VoidVDP bitmap-layer example.
 *
 * Compiled for the Z80 with the LLVM-Z80 backend through z88dk's
 * `-compiler=llvmz80` and the in-tree `+z80neo` classic-library target:
 *
 *   zcc +z80neo -compiler=llvmz80 -O2 vdpdemo.c -o vdpdemo -create-app
 *
 * The VoidVDP bitmap layer is 640x256 with 16 colours (CGA order).  The
 * program draws sixteen hardware-filled colour bars, a rotating line fan,
 * a sliding box and a short per-pixel colour ramp, then quits on q / ESC.
 *
 * All maths is integer: the Z80 runs at ~128 kHz, so there is no <math.h>
 * (the demos' fixed-point sine table is reused from ../demos/common.h).
 */

#include <stdint.h>
#include <z80neo.h>
#include "../demos/common.h"

#define W  640u
#define H  256u

#define CX 320
#define CY 128
#define R  110

int main(void)
{
	uint8_t t = 0;

	/* Enter bitmap mode (this also clears the layer). */
	z80neo_gfx_bitmap(1);

	for (;;) {
		uint8_t i;

		z80neo_gfx_clg();

		/* Sixteen hardware-filled colour bars along the top. */
		for (i = 0; i < 16; i++) {
			z80neo_gfx_color(i, 0);
			z80neo_gfx_fill((uint16_t)(i * 40u), 0, 40, 24, 1);
		}

		/* Rotating fan: 12 spokes, colour cycling with the frame. */
		for (i = 0; i < 12; i++) {
			uint8_t a = (uint8_t)(t + i * 21u);
			int ex = CX + (demo_icos(a) * R) / 256;
			int ey = CY + (demo_isin(a) * R) / 256;

			z80neo_gfx_color((uint8_t)(((t >> 1) + i) & 0x0f), 0);
			z80neo_gfx_line(CX, CY, (uint16_t)ex, (uint16_t)ey, 1);
		}

		/* A box sliding across the bottom. */
		z80neo_gfx_color(15, 0);
		z80neo_gfx_fill((uint16_t)((t * 3u) % (W - 64u)), 200, 64, 40, 1);

		/* Per-pixel colour ramp (one display colour per pixel). */
		for (i = 0; i < 64; i++)
			z80neo_gfx_pixel((uint16_t)(288 + i), 60, (uint8_t)(t + i) & 0x0f);

		/* Rough frame pacing for the 128 kHz CPU. */
		for (volatile uint16_t d = 0; d < 2000; d++)
			;

		t++;
		if (demo_quit())
			break;
	}

	z80neo_gfx_bitmap(0);
	return 0;
}
