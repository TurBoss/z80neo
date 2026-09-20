/*
 * balls.c - bouncing colour blocks on the VoidVDP bitmap layer.
 *
 * Five 16x16 blocks bounce around; each is erased with a clear fill and
 * redrawn with a bright colour.  Press q or ESC to quit.
 */

#include <stdint.h>
#include <z80neo.h>
#include "common.h"

#define NBALLS 5
#define BSIZE 16

static int bx[NBALLS], by[NBALLS];
static int vx[NBALLS], vy[NBALLS];
static uint8_t bc[NBALLS];

int main(void)
{
	for (int i = 0; i < NBALLS; i++) {
		bx[i] = 20 + (int)(demo_rand() % 560u);
		by[i] = 20 + (int)(demo_rand() % 200u);
		vx[i] = (int)(demo_rand() % 5u) - 2;
		vy[i] = (int)(demo_rand() % 5u) - 2;
		if (!vx[i])
			vx[i] = 2;
		if (!vy[i])
			vy[i] = 2;
		bc[i] = (uint8_t)(9 + (i * 2));
	}

	z80neo_gfx_bitmap(1);

	for (;;) {
		for (int i = 0; i < NBALLS; i++) {
			z80neo_gfx_fill((uint16_t)bx[i], (uint16_t)by[i], BSIZE, BSIZE, 0);

			bx[i] += vx[i];
			by[i] += vy[i];
			if (bx[i] < 0) {
				bx[i] = 0;
				vx[i] = -vx[i];
			} else if (bx[i] > 640 - BSIZE) {
				bx[i] = 640 - BSIZE;
				vx[i] = -vx[i];
			}
			if (by[i] < 0) {
				by[i] = 0;
				vy[i] = -vy[i];
			} else if (by[i] > 256 - BSIZE) {
				by[i] = 256 - BSIZE;
				vy[i] = -vy[i];
			}

			z80neo_gfx_color(bc[i], 0);
			z80neo_gfx_fill((uint16_t)bx[i], (uint16_t)by[i], BSIZE, BSIZE, 1);
		}
		if (demo_quit())
			break;
	}

	z80neo_gfx_bitmap(0);
	return 0;
}
