/*
 * cube.c - rotating 3D wireframe cube on the VoidVDP bitmap layer.
 *
 * Twelve hardware line commands per frame.  Press q or ESC to quit.
 */

#include <stdint.h>
#include <z80neo.h>
#include "common.h"

static const int16_t vx[8] = { -256, -256, 256, 256, -256, -256, 256, 256 };
static const int16_t vy[8] = { -256, 256, 256, -256, -256, 256, 256, -256 };
static const int16_t vz[8] = { -256, -256, -256, -256, 256, 256, 256, 256 };
static const uint8_t e0[12] = { 0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3 };
static const uint8_t e1[12] = { 1, 2, 3, 0, 5, 6, 7, 4, 4, 5, 6, 7 };

int main(void)
{
	int16_t px[8], py[8];
	uint8_t a = 0, b = 0;

	z80neo_gfx_bitmap(1);

	for (;;) {
		int cy = demo_icos(a), sy = demo_isin(a);
		int cx = demo_icos(b), sx = demo_isin(b);

		for (int i = 0; i < 8; i++) {
			long x = vx[i], y = vy[i], z = vz[i];
			long x1 = (x * cy - z * sy) >> 8;
			long z1 = (x * sy + z * cy) >> 8;
			long y1 = (y * cx - z1 * sx) >> 8;
			px[i] = (int16_t)(320 + (x1 >> 2));
			py[i] = (int16_t)(128 - (y1 >> 2));
		}

		z80neo_gfx_color((uint8_t)(8 + ((a >> 4) & 7)), 0);
		z80neo_gfx_clg();

		for (int i = 0; i < 12; i++)
			z80neo_gfx_line(px[e0[i]], py[e0[i]], px[e1[i]], py[e1[i]], 1);

		a = (uint8_t)(a + 2);
		b = (uint8_t)(b + 1);
		if (demo_quit())
			break;
	}

	z80neo_gfx_bitmap(0);
	return 0;
}
