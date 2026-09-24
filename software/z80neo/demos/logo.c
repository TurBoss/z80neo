/*
 * logo.c - animated "Z80NEO" neon wordmark on the VoidVDP bitmap layer.
 *
 * A 7-segment vector wordmark: each stroke is filled twice, once in a dim
 * variant of the colour (halo) and once in the bright colour (tube), so it
 * reads as neon on black.  A rainbow wave chases across the six glyphs and a
 * highlight sweeps along the underline.  Works on an RP2350 VDP (DVI/VGA) and
 * the PC sdl_client; press q or ESC to quit.
 *
 * Uses the <z80neo.h> gfx calls, i.e. the ESC g console protocol.
 */

#include <stdint.h>
#include <z80neo.h>
#include "common.h"

#define GLYPH_W 64
#define GLYPH_H 104
#define GLYPH_T 14
#define GAP     16
#define GLYPH_N 6
#define TOP_Y   76

/* Bright palette entries; the halo is drawn in (colour - 8). */
static const uint8_t wave[GLYPH_N] = { 9, 10, 11, 12, 13, 14 };

static const uint8_t kinds[GLYPH_N] = { 'Z', '8', '0', 'N', 'E', 'O' };

#define SA 0x01
#define SB 0x02
#define SC 0x04
#define SD 0x08
#define SE 0x10
#define SF 0x20
#define SG 0x40

static uint8_t glyph_segs(uint8_t k)
{
	switch (k) {
	case 'Z':
		return SA | SB | SG | SE | SD;
	case '8':
		return SA | SB | SC | SD | SE | SF | SG;
	case '0':
	case 'O':
		return SA | SB | SC | SD | SE | SF;
	case 'N':
		return SB | SC | SE | SF;
	case 'E':
		return SA | SF | SG | SE | SD;
	}
	return 0;
}

/* One stroke rectangle plus its halo, in colour `c`. */
static void stroke(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t c)
{
	if (c > 8) {
		z80neo_gfx_color((uint8_t)(c - 8), 0);
		z80neo_gfx_fill((uint16_t)(x - 3), (uint16_t)(y - 3),
				(uint16_t)(w + 6), (uint16_t)(h + 6), 1);
	}
	z80neo_gfx_color(c, 0);
	z80neo_gfx_fill(x, y, w, h, 1);
}

static void draw_glyph(uint16_t x, uint8_t k, uint8_t c)
{
	uint8_t s = glyph_segs(k);
	uint16_t hh = GLYPH_H / 2;
	uint16_t vh = (uint16_t)(hh - GLYPH_T);
	uint16_t rx = (uint16_t)(x + GLYPH_W - GLYPH_T);
	uint16_t my = (uint16_t)(TOP_Y + hh - GLYPH_T / 2);

	if (s & SA)
		stroke((uint16_t)(x + GLYPH_T), TOP_Y, (uint16_t)(GLYPH_W - 2 * GLYPH_T), GLYPH_T, c);
	if (s & SG)
		stroke((uint16_t)(x + GLYPH_T), my, (uint16_t)(GLYPH_W - 2 * GLYPH_T), GLYPH_T, c);
	if (s & SD)
		stroke((uint16_t)(x + GLYPH_T), (uint16_t)(TOP_Y + GLYPH_H - GLYPH_T),
		       (uint16_t)(GLYPH_W - 2 * GLYPH_T), GLYPH_T, c);
	if (s & SF)
		stroke(x, (uint16_t)(TOP_Y + GLYPH_T), GLYPH_T, vh, c);
	if (s & SE)
		stroke(x, (uint16_t)(TOP_Y + hh), GLYPH_T, vh, c);
	if (s & SB)
		stroke(rx, (uint16_t)(TOP_Y + GLYPH_T), GLYPH_T, vh, c);
	if (s & SC)
		stroke(rx, (uint16_t)(TOP_Y + hh), GLYPH_T, vh, c);
	if (k == 'N') {
		// Single gfx lines are 1px; stack parallel lines (shifted in X, which
		// the end bars hide) so the diagonal matches the stroke width and glow.
		int s;
		if (c > 8) {
			z80neo_gfx_color((uint8_t)(c - 8), 0);
			for (s = -(GLYPH_T / 2 + 3); s <= GLYPH_T / 2 + 3; s++)
				z80neo_gfx_line((uint16_t)(x + GLYPH_T + s),
						(uint16_t)(TOP_Y + GLYPH_T),
						(uint16_t)(x + GLYPH_W - GLYPH_T + s),
						(uint16_t)(TOP_Y + GLYPH_H - GLYPH_T), 1);
		}
		z80neo_gfx_color(c, 0);
		for (s = -(GLYPH_T / 2); s <= GLYPH_T / 2; s++)
			z80neo_gfx_line((uint16_t)(x + GLYPH_T + s),
					(uint16_t)(TOP_Y + GLYPH_T),
					(uint16_t)(x + GLYPH_W - GLYPH_T + s),
					(uint16_t)(TOP_Y + GLYPH_H - GLYPH_T), 1);
	}
}

int main(void)
{
	const uint16_t bw = (uint16_t)(GLYPH_N * GLYPH_W + (GLYPH_N - 1) * GAP);
	const uint16_t x0 = (uint16_t)((Z80NEO_GFX_WIDTH - bw) / 2);
	const uint16_t by = (uint16_t)(TOP_Y + GLYPH_H + 14);
	uint16_t sweep = 0;
	uint8_t phase = 0;
	uint8_t i;

	z80neo_gfx_color(0, 0);
	z80neo_gfx_bitmap(1);           /* also clears to the bg colour */

	for (;;) {
		for (i = 0; i < GLYPH_N; i++) {
			uint16_t gx = (uint16_t)(x0 + i * (GLYPH_W + GAP));
			draw_glyph(gx, kinds[i], wave[(uint8_t)((i + phase) % GLYPH_N)]);
		}

		/* Underline: dim rail with a bright highlight sliding along it. */
		z80neo_gfx_color(8, 0);
		z80neo_gfx_fill(x0, by, bw, 4, 1);
		z80neo_gfx_color(15, 0);
		{
			uint16_t sw = (uint16_t)(bw / 5);
			uint16_t sx = (uint16_t)(x0 + sweep);
			if (sx + sw > x0 + bw)
				sw = (uint16_t)(x0 + bw - sx);
			z80neo_gfx_fill(sx, by, sw, 4, 1);
		}
		sweep = (uint16_t)(sweep + 12);
		if (sweep > bw)
			sweep = 0;

		phase = (uint8_t)((phase + 1) % GLYPH_N);
		if (demo_quit())
			break;
	}

	z80neo_gfx_bitmap(0);
	return 0;
}
