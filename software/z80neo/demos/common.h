/*
 * Shared integer helpers for the z80neo bare-metal demos.
 *
 * The Z80 runs at ~128 kHz, so the demos use fixed-point integer maths
 * (no <math.h>) and a tiny LCG instead of rand().
 */
#ifndef DEMO_COMMON_H
#define DEMO_COMMON_H

#include <stdint.h>
#include <stdio.h>

/* sin(i*pi/128)*256 for i = 0..64 */
static const int demo_sintab[65] = {
	0, 6, 13, 19, 25, 31, 38, 44,
	50, 56, 62, 68, 74, 80, 86, 92,
	98, 104, 109, 115, 121, 126, 132, 137,
	142, 147, 152, 157, 162, 167, 172, 177,
	181, 185, 190, 194, 198, 202, 206, 209,
	213, 216, 220, 223, 226, 229, 231, 234,
	237, 239, 241, 243, 245, 247, 248, 250,
	251, 252, 253, 254, 255, 255, 256, 256,
	256,
};

/* angle 0..255 is one full turn; result is -256..256 */
static int demo_isin(uint8_t a)
{
	if (a <= 64)
		return demo_sintab[a];
	if (a <= 128)
		return demo_sintab[128 - a];
	if (a <= 192)
		return -demo_sintab[a - 128];
	return -demo_sintab[256 - a];
}

static int demo_icos(uint8_t a)
{
	return demo_isin((uint8_t)(a + 64));
}

static uint16_t demo_seed = 0x1234;

static uint16_t demo_rand(void)
{
	demo_seed = (uint16_t)(demo_seed * 25173u + 13849u);
	return demo_seed;
}

/* Non-blocking key read; 0 when none, and 'q'/ESC quit. */
static int demo_quit(void)
{
	int c = getk();
	return c == 'q' || c == 'Q' || c == 27;
}

#endif
