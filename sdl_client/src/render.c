#include "render.h"

#include "font8x16.h"
static const uint32_t palette[16] = {
	0xFF000000, 0xFF0000AA, 0xFF00AA00, 0xFF00AAAA, 0xFFAA0000, 0xFFAA00AA,
	0xFFAA5500, 0xFFAAAAAA, 0xFF555555, 0xFF5555FF, 0xFF55FF55, 0xFF55FFFF,
	0xFFFF5555, 0xFFFF55FF, 0xFFFFFF55, 0xFFFFFFFF,
};

static const unsigned char *glyph_for(uint32_t cp) {
	int lo = 0, hi = FONT8X16_MAP_LEN - 1;
	while (lo <= hi) {
		int mid = (lo + hi) / 2;
		unsigned short c = font8x16_map[mid].cp;
		if (c == cp)
			return font8x16_bitmap + font8x16_map[mid].glyph * FONT8X16_H;
		if (c < cp)
			lo = mid + 1;
		else
			hi = mid - 1;
	}
	if (lo < FONT8X16_MAP_LEN && font8x16_map[lo].cp == cp)
		return font8x16_bitmap + font8x16_map[lo].glyph * FONT8X16_H;
	return font8x16_bitmap + '?' * FONT8X16_H;
}
static void render_gfx(const Terminal *t, uint32_t *pixels, int cols,
					   int rows) {
	int stride = cols * RENDER_CELL_W;
	int frame_h = rows * RENDER_CELL_H;
	int yoff = (frame_h - TERM_GFX_HEIGHT) / 2;
	if (yoff < 0)
		yoff = 0;
	const uint8_t *gfx = terminal_gfx_pixels(t);
	uint32_t outside = palette[terminal_gfx_bg(t) & 15];

	for (int y = 0; y < frame_h; y++) {
		int gy = y - yoff;
		uint32_t *dst = pixels + (size_t)y * stride;
		for (int x = 0; x < stride; x++) {
			if (gy < 0 || gy >= TERM_GFX_HEIGHT || x >= TERM_GFX_WIDTH) {
				dst[x] = outside;
				continue;
			}
			dst[x] = palette[gfx[gy * TERM_GFX_WIDTH + x] & 15];
		}
	}
}

void render_frame(const Terminal *t, uint32_t *pixels, bool blink_on,
				  bool flash) {
	int cols = terminal_cols(t);
	int rows = terminal_rows(t);
	const TermCell *cells = terminal_cells(t);

	if (terminal_gfx_active(t)) {
		render_gfx(t, pixels, cols, rows);
		return;
	}
	int cur_x = terminal_cursor_x(t);
	int cur_y = terminal_cursor_y(t);
	bool cur_vis = terminal_cursor_visible(t);
	bool inverted = terminal_screen_inverted(t) ^ flash;
	int stride = cols * RENDER_CELL_W;

	for (int y = 0; y < rows; y++) {
		for (int x = 0; x < cols; x++) {
			const TermCell *c = &cells[y * cols + x];
			int fg_i = c->fg & 15;
			int bg_i = c->bg & 15;
			bool bold = c->attr & TERM_ATTR_BOLD;
			bool underline = c->attr & TERM_ATTR_UNDERLINE;
			bool blink = c->attr & TERM_ATTR_BLINK;

			if (bold)
				fg_i |= 8;
			if (c->attr & TERM_ATTR_INVERSE) {
				int tmp = fg_i;
				fg_i = bg_i;
				bg_i = tmp;
			}
			if (inverted) {
				int tmp = fg_i;
				fg_i = bg_i;
				bg_i = tmp;
			}
			if (blink && !blink_on)
				fg_i = bg_i;
			if (cur_vis && blink_on && x == cur_x && y == cur_y) {
				int tmp = fg_i;
				fg_i = bg_i;
				bg_i = tmp;
			}

			uint32_t fg = palette[fg_i & 15];
			uint32_t bg = palette[bg_i & 15];
			const unsigned char *glyph = glyph_for(c->ch);

			for (int r = 0; r < FONT8X16_H; r++) {
				unsigned char bits = glyph[r];
				if (bold)
					bits |= bits >> 1;
				if (underline && r == FONT8X16_H - 2)
					bits = 0xFF;
				uint32_t *dst =
					pixels + (y * FONT8X16_H + r) * stride + x * RENDER_CELL_W;
				for (int b = 0; b < RENDER_CELL_W; b++)
					dst[b] = (bits & (0x80 >> b)) ? fg : bg;
			}
		}
	}
}
