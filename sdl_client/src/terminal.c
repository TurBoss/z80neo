#include "terminal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

enum { CS_US, CS_UK, CS_GRAPHICS };

enum {
	TS_NORMAL,
	TS_WAITBRACKET,
	TS_STARTCHAR,
	TS_READPARAM,
	TS_HASH,
	TS_READCHAR,
	TS_GFXCMD
};

struct Terminal {
	int cols, rows;
	TermCell *cells;
	int cx, cy;
	TermCell pen;
	bool pending_wrap;
	bool cursor_visible;
	bool screen_inverted;
	bool origin_mode;
	bool autowrap;
	bool insert_mode;
	bool vt52_mode;
	int scroll_top, scroll_bottom;
	bool tabs[256];
	uint8_t g0, g1;
	uint8_t *charset;

	int scx, scy;
	TermCell spen;
	bool spending_wrap, sorigin;
	uint8_t sg0, sg1;

	int state;
	uint8_t start_char;
	int nparams;
	uint8_t params[16];

	uint8_t gfx_buf[TERM_GFX_STRIDE * TERM_GFX_HEIGHT];
	uint8_t gfx_cell_fg[TERM_GFX_STRIDE * TERM_GFX_ROWS];
	uint8_t gfx_cell_bg[TERM_GFX_STRIDE * TERM_GFX_ROWS];
	bool gfx_active;
	uint8_t gfx_fg, gfx_bg;
	uint8_t gfx_cmd;
	int gfx_need;
	int gfx_index;
	uint8_t gfx_params[9];

	uint8_t out[TERM_OUT_MAX];
	size_t out_len;
	unsigned bell_count;
};

static const uint32_t cp437[128] = {
	0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7, 0x00EA,
	0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5, 0x00C9, 0x00E6,
	0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9, 0x00FF, 0x00D6, 0x00DC,
	0x00A2, 0x00A3, 0x00A5, 0x20A7, 0x0192, 0x00E1, 0x00ED, 0x00F3, 0x00FA,
	0x00F1, 0x00D1, 0x00AA, 0x00BA, 0x00BF, 0x2310, 0x00AC, 0x00BD, 0x00BC,
	0x00A1, 0x00AB, 0x00BB, 0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561,
	0x2562, 0x2556, 0x2555, 0x2563, 0x2551, 0x2557, 0x255D, 0x255C, 0x255B,
	0x2510, 0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x255E, 0x255F,
	0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x2567, 0x2568,
	0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256B, 0x256A, 0x2518,
	0x250C, 0x2588, 0x2584, 0x258C, 0x2590, 0x2580, 0x03B1, 0x00DF, 0x0393,
	0x03C0, 0x03A3, 0x03C3, 0x00B5, 0x03C4, 0x03A6, 0x0398, 0x03A9, 0x03B4,
	0x221E, 0x03C6, 0x03B5, 0x2229, 0x2261, 0x00B1, 0x2265, 0x2264, 0x2320,
	0x2321, 0x00F7, 0x2248, 0x00B0, 0x2219, 0x00B7, 0x221A, 0x207F, 0x00B2,
	0x25A0, 0x00A0,
};

static void out_append(Terminal *t, const void *data, size_t len) {
	const uint8_t *p = data;
	while (len-- && t->out_len < TERM_OUT_MAX)
		t->out[t->out_len++] = *p++;
}

static void send_str(Terminal *t, const char *s) {
	out_append(t, s, strlen(s));
}

static void fill_region(Terminal *t, int x0, int y0, int x1, int y1,
						uint32_t ch, uint8_t attr) {
	if (x0 < 0)
		x0 = 0;
	if (y0 < 0)
		y0 = 0;
	if (x1 >= t->cols)
		x1 = t->cols - 1;
	if (y1 >= t->rows)
		y1 = t->rows - 1;
	for (int y = y0; y <= y1; y++)
		for (int x = x0; x <= x1; x++) {
			TermCell *c = &t->cells[y * t->cols + x];
			c->ch = ch;
			c->fg = t->pen.fg;
			c->bg = t->pen.bg;
			c->attr = attr;
		}
}

static void scroll_region(Terminal *t, int top, int bottom, int n) {
	if (top < 0)
		top = 0;
	if (bottom >= t->rows)
		bottom = t->rows - 1;
	if (top >= bottom)
		return;
	int height = bottom - top + 1;

	if (n > 0) {
		if (n > height)
			n = height;
		memmove(&t->cells[top * t->cols], &t->cells[(top + n) * t->cols],
				(size_t)(height - n) * t->cols * sizeof(TermCell));
		fill_region(t, 0, bottom - n + 1, t->cols - 1, bottom, ' ', 0);
	} else if (n < 0) {
		n = -n;
		if (n > height)
			n = height;
		memmove(&t->cells[(top + n) * t->cols], &t->cells[top * t->cols],
				(size_t)(height - n) * t->cols * sizeof(TermCell));
		fill_region(t, 0, top, t->cols - 1, top + n - 1, ' ', 0);
	}
}

static void index_down(Terminal *t) {
	if (t->cy == t->scroll_bottom)
		scroll_region(t, t->scroll_top, t->scroll_bottom, 1);
	else if (t->cy < t->rows - 1)
		t->cy++;
}

static void reverse_index(Terminal *t) {
	if (t->cy == t->scroll_top)
		scroll_region(t, t->scroll_top, t->scroll_bottom, -1);
	else if (t->cy > 0)
		t->cy--;
}

static void move_cursor(Terminal *t, int row, int col) {
	int top = t->origin_mode ? t->scroll_top : 0;
	int bot = t->origin_mode ? t->scroll_bottom : t->rows - 1;
	if (row < top)
		row = top;
	if (row > bot)
		row = bot;
	if (col < 0)
		col = 0;
	if (col >= t->cols)
		col = t->cols - 1;
	t->cy = row;
	t->cx = col;
	t->pending_wrap = false;
}

static void insert_chars(Terminal *t, int n) {
	if (t->cx >= t->cols || n <= 0)
		return;
	if (n > t->cols - t->cx)
		n = t->cols - t->cx;
	memmove(&t->cells[t->cy * t->cols + t->cx + n],
			&t->cells[t->cy * t->cols + t->cx],
			(size_t)(t->cols - t->cx - n) * sizeof(TermCell));
	fill_region(t, t->cx, t->cy, t->cx + n - 1, t->cy, ' ', 0);
}

static void delete_chars(Terminal *t, int n) {
	if (t->cx >= t->cols || n <= 0)
		return;
	if (n > t->cols - t->cx)
		n = t->cols - t->cx;
	memmove(&t->cells[t->cy * t->cols + t->cx],
			&t->cells[t->cy * t->cols + t->cx + n],
			(size_t)(t->cols - t->cx - n) * sizeof(TermCell));
	fill_region(t, t->cols - n, t->cy, t->cols - 1, t->cy, ' ', 0);
}

static uint32_t dec_graphics(uint8_t c) {
	static const uint32_t g[0x7f - 0x5f + 1] = {
		0x0020, 0x25C6, 0x2592, 0x2409, 0x240C, 0x240D, 0x240A, 0x00B0,
		0x00B1, 0x2424, 0x240B, 0x2518, 0x2510, 0x250C, 0x2514, 0x253C,
		0x23BA, 0x23BB, 0x2500, 0x23BC, 0x23BD, 0x251C, 0x2524, 0x2534,
		0x252C, 0x2502, 0x2264, 0x2265, 0x03C0, 0x2260, 0x00A3, 0x00B7,
	};
	return g[c - 0x5f];
}

static uint32_t map_charset(Terminal *t, uint8_t c) {
	uint8_t cs = *t->charset;
	if (cs == CS_UK && c == 0x23)
		return 0x00A3;
	if (cs == CS_GRAPHICS && c >= 0x5f && c <= 0x7e)
		return dec_graphics(c);
	if (c < 128)
		return c;
	return cp437[c - 128];
}

static void put_char(Terminal *t, uint32_t ch) {
	if (t->pending_wrap) {
		t->cx = 0;
		index_down(t);
		t->pending_wrap = false;
	}
	if (t->insert_mode)
		insert_chars(t, 1);

	TermCell *c = &t->cells[t->cy * t->cols + t->cx];
	c->ch = ch;
	c->fg = t->pen.fg;
	c->bg = t->pen.bg;
	c->attr = t->pen.attr;

	if (t->autowrap && t->cx == t->cols - 1)
		t->pending_wrap = true;
	else if (t->cx < t->cols - 1)
		t->cx++;
}

static void save_cursor(Terminal *t) {
	t->scx = t->cx;
	t->scy = t->cy;
	t->spen = t->pen;
	t->spending_wrap = t->pending_wrap;
	t->sorigin = t->origin_mode;
	t->sg0 = t->g0;
	t->sg1 = t->g1;
}

static void restore_cursor(Terminal *t) {
	move_cursor(t, t->scy, t->scx);
	t->pen = t->spen;
	t->pending_wrap = t->spending_wrap;
	t->origin_mode = t->sorigin;
	t->g0 = t->sg0;
	t->g1 = t->sg1;
}

static void reset_terminal(Terminal *t) {
	t->pen.ch = ' ';
	t->pen.fg = 7;
	t->pen.bg = 0;
	t->pen.attr = 0;
	t->cursor_visible = true;
	t->screen_inverted = false;
	t->origin_mode = false;
	t->autowrap = true;
	t->insert_mode = false;
	t->vt52_mode = false;
	t->scroll_top = 0;
	t->scroll_bottom = t->rows - 1;
	t->g0 = CS_US;
	t->g1 = CS_GRAPHICS;
	t->charset = &t->g0;
	t->pending_wrap = false;
	t->cx = 0;
	t->cy = 0;
	memset(t->tabs, 0, sizeof(t->tabs));
	for (int i = 0; i < t->cols; i += 8)
		t->tabs[i] = true;
	t->state = TS_NORMAL;
	t->gfx_active = false;
	t->gfx_fg = 15;
	t->gfx_bg = 0;
	t->gfx_cmd = 0;
	t->gfx_need = -1;
	t->gfx_index = 0;
	memset(t->gfx_buf, 0, sizeof(t->gfx_buf));
	for (size_t i = 0; i < sizeof(t->gfx_cell_fg); i++) {
		t->gfx_cell_fg[i] = t->gfx_fg;
		t->gfx_cell_bg[i] = t->gfx_bg;
	}
}

/* ---- VoidVDP bitmap layer -------------------------------------------- */

static void gfx_set_cell_color(Terminal *t, int x, int y) {
	int col = x >> 3;
	int row = y / TERM_GFX_CELL_H;
	if (col < 0 || col >= TERM_GFX_STRIDE || row < 0 || row >= TERM_GFX_ROWS)
		return;
	size_t idx = (size_t)row * TERM_GFX_STRIDE + col;
	t->gfx_cell_fg[idx] = t->gfx_fg;
	t->gfx_cell_bg[idx] = t->gfx_bg;
}

static void gfx_fill_cells(Terminal *t) {
	for (size_t i = 0; i < sizeof(t->gfx_cell_fg); i++) {
		t->gfx_cell_fg[i] = t->gfx_fg;
		t->gfx_cell_bg[i] = t->gfx_bg;
	}
}

static void gfx_clear(Terminal *t) {
	memset(t->gfx_buf, 0, sizeof(t->gfx_buf));
	gfx_fill_cells(t);
}

static void gfx_enable(Terminal *t, bool on) {
	if (on) {
		gfx_clear(t);
		t->gfx_active = true;
	} else {
		t->gfx_active = false;
	}
}

static void gfx_pixel(Terminal *t, int x, int y, bool on) {
	if (x < 0 || x >= TERM_GFX_WIDTH || y < 0 || y >= TERM_GFX_HEIGHT)
		return;
	uint8_t *p = &t->gfx_buf[y * TERM_GFX_STRIDE + (x >> 3)];
	uint8_t m = (uint8_t)(1 << (x & 7));
	if (on)
		*p |= m;
	else
		*p &= (uint8_t)~m;
	gfx_set_cell_color(t, x, y);
}

static void gfx_line(Terminal *t, int x0, int y0, int x1, int y1, bool on) {
	int dx = x1 > x0 ? x1 - x0 : x0 - x1;
	int dy = y1 > y0 ? y1 - y0 : y0 - y1;
	int sx = x0 < x1 ? 1 : -1;
	int sy = y0 < y1 ? 1 : -1;
	int err = dx - dy;

	for (;;) {
		gfx_pixel(t, x0, y0, on);
		if (x0 == x1 && y0 == y1)
			break;
		int e2 = 2 * err;
		if (e2 > -dy) {
			err -= dy;
			x0 += sx;
		}
		if (e2 < dx) {
			err += dx;
			y0 += sy;
		}
	}
}

static void gfx_fill(Terminal *t, int x, int y, int w, int h, bool on) {
	for (int j = 0; j < h; j++)
		for (int i = 0; i < w; i++)
			gfx_pixel(t, x + i, y + j, on);
}

static void gfx_exec(Terminal *t) {
	switch (t->gfx_cmd) {
	case 0x01:
		gfx_enable(t, t->gfx_params[0] != 0);
		break;
	case 0x02:
		gfx_clear(t);
		break;
	case 0x03:
		t->gfx_fg = t->gfx_params[0] & 15;
		t->gfx_bg = t->gfx_params[1] & 15;
		break;
	case 0x04:
		gfx_pixel(t, t->gfx_params[0] | (t->gfx_params[1] << 8),
				  t->gfx_params[2] | (t->gfx_params[3] << 8),
				  t->gfx_params[4] != 0);
		break;
	case 0x05:
		gfx_line(t, t->gfx_params[0] | (t->gfx_params[1] << 8),
				 t->gfx_params[2] | (t->gfx_params[3] << 8),
				 t->gfx_params[4] | (t->gfx_params[5] << 8),
				 t->gfx_params[6] | (t->gfx_params[7] << 8),
				 t->gfx_params[8] != 0);
		break;
	case 0x06:
		gfx_fill(t, t->gfx_params[0] | (t->gfx_params[1] << 8),
				 t->gfx_params[2] | (t->gfx_params[3] << 8),
				 t->gfx_params[4] | (t->gfx_params[5] << 8),
				 t->gfx_params[6] | (t->gfx_params[7] << 8),
				 t->gfx_params[8] != 0);
		break;
	}
}

static void clear_screen(Terminal *t) {
	fill_region(t, 0, 0, t->cols - 1, t->rows - 1, ' ', 0);
	t->cx = 0;
	t->cy = 0;
	t->pending_wrap = false;
	t->scroll_top = 0;
	t->scroll_bottom = t->rows - 1;
	t->origin_mode = false;
}

static void process_text(Terminal *t, uint8_t c) {
	switch (c) {
	case 5:
		break;
	case 7:
		t->bell_count++;
		break;
	case 8:
	case 127:
		if (t->cx > 0) {
			t->cx--;
			t->pending_wrap = false;
		} else if (t->cy > 0) {
			t->cy--;
			t->cx = t->cols - 1;
			t->pending_wrap = false;
		}
		break;
	case '\t': {
		int col = t->cx + 1;
		while (col < t->cols - 1 && !t->tabs[col])
			col++;
		move_cursor(t, t->cy, col);
		break;
	}
	case '\n':
	case 11:
	case 12:
		index_down(t);
		break;
	case '\r':
		move_cursor(t, t->cy, 0);
		break;
	case 14:
		t->charset = &t->g1;
		break;
	case 15:
		t->charset = &t->g0;
		break;
	default:
		if (c >= 32)
			put_char(t, map_charset(t, c));
		break;
	}
}

static void sgr(Terminal *t, int nparams, uint8_t *params) {
	for (int i = 0; i < nparams; i++) {
		int p = params[i];
		if (p == 0) {
			t->pen.fg = 7;
			t->pen.bg = 0;
			t->pen.attr = 0;
		} else if (p == 1) {
			t->pen.attr |= TERM_ATTR_BOLD;
		} else if (p == 4) {
			t->pen.attr |= TERM_ATTR_UNDERLINE;
		} else if (p == 5) {
			t->pen.attr |= TERM_ATTR_BLINK;
		} else if (p == 7) {
			t->pen.attr |= TERM_ATTR_INVERSE;
		} else if (p == 22) {
			t->pen.attr &= ~TERM_ATTR_BOLD;
		} else if (p == 24) {
			t->pen.attr &= ~TERM_ATTR_UNDERLINE;
		} else if (p == 25) {
			t->pen.attr &= ~TERM_ATTR_BLINK;
		} else if (p == 27) {
			t->pen.attr &= ~TERM_ATTR_INVERSE;
		} else if (p >= 30 && p <= 37) {
			t->pen.fg = p - 30;
		} else if (p == 38 && i + 2 < nparams && params[i + 1] == 5) {
			t->pen.fg = params[i + 2] & 15;
			i += 2;
		} else if (p == 39) {
			t->pen.fg = 7;
		} else if (p >= 40 && p <= 47) {
			t->pen.bg = p - 40;
		} else if (p == 48 && i + 2 < nparams && params[i + 1] == 5) {
			t->pen.bg = params[i + 2] & 15;
			i += 2;
		} else if (p == 49) {
			t->pen.bg = 0;
		} else if (p >= 90 && p <= 97) {
			t->pen.fg = (p - 90) | 8;
		} else if (p >= 100 && p <= 107) {
			t->pen.bg = (p - 100) | 8;
		}
	}
}

static void csi_dispatch(Terminal *t, uint8_t start_char, uint8_t final_char,
						 int nparams, uint8_t *params) {
	if (final_char == 'l' || final_char == 'h') {
		bool on = final_char == 'h';
		if (start_char == '?') {
			switch (params[0]) {
			case 2:
				if (!on) {
					reset_terminal(t);
					t->vt52_mode = true;
				}
				break;
			case 3:
				clear_screen(t);
				break;
			case 5:
				t->screen_inverted = on;
				break;
			case 6:
				t->origin_mode = on;
				move_cursor(t, t->scroll_top, 0);
				break;
			case 7:
				t->autowrap = on;
				break;
			case 25:
				t->cursor_visible = on;
				break;
			}
		} else {
			if (params[0] == 4)
				t->insert_mode = on;
		}
	} else if (final_char == 'J') {
		switch (params[0]) {
		case 0:
			fill_region(t, t->cx, t->cy, t->cols - 1, t->cy, ' ', 0);
			fill_region(t, 0, t->cy + 1, t->cols - 1, t->rows - 1, ' ', 0);
			break;
		case 1:
			fill_region(t, 0, 0, t->cols - 1, t->cy - 1, ' ', 0);
			fill_region(t, 0, t->cy, t->cx, t->cy, ' ', 0);
			break;
		case 2:
			fill_region(t, 0, 0, t->cols - 1, t->rows - 1, ' ', 0);
			break;
		}
	} else if (final_char == 'K') {
		switch (params[0]) {
		case 0:
			fill_region(t, t->cx, t->cy, t->cols - 1, t->cy, ' ', 0);
			break;
		case 1:
			fill_region(t, 0, t->cy, t->cx, t->cy, ' ', 0);
			break;
		case 2:
			fill_region(t, 0, t->cy, t->cols - 1, t->cy, ' ', 0);
			break;
		}
	} else if (final_char == 'A') {
		move_cursor(t, t->cy - MAX(1, params[0]), t->cx);
	} else if (final_char == 'B') {
		move_cursor(t, t->cy + MAX(1, params[0]), t->cx);
	} else if (final_char == 'C' || final_char == 'a') {
		move_cursor(t, t->cy, t->cx + MAX(1, params[0]));
	} else if (final_char == 'D' || final_char == 'j') {
		move_cursor(t, t->cy, t->cx - MAX(1, params[0]));
	} else if (final_char == 'E' || final_char == 'e') {
		move_cursor(t, t->cy + MAX(1, params[0]), 0);
	} else if (final_char == 'F' || final_char == 'k') {
		move_cursor(t, t->cy - MAX(1, params[0]), 0);
	} else if (final_char == 'd') {
		int top = t->origin_mode ? t->scroll_top : 0;
		move_cursor(t, top + MAX(1, params[0]) - 1, t->cx);
	} else if (final_char == 'G' || final_char == '`') {
		move_cursor(t, t->cy, MAX(1, params[0]) - 1);
	} else if (final_char == 'H' || final_char == 'f') {
		int top = t->origin_mode ? t->scroll_top : 0;
		move_cursor(t, top + MAX(params[0], 1) - 1,
					nparams < 2 ? 0 : MAX(params[1], 1) - 1);
	} else if (final_char == 'I') {
		int n = MAX(1, params[0]);
		int col = t->cx + 1;
		while (n > 0 && col < t->cols - 1) {
			while (col < t->cols - 1 && !t->tabs[col])
				col++;
			n--;
		}
		move_cursor(t, t->cy, col);
	} else if (final_char == 'Z') {
		int n = MAX(1, params[0]);
		int col = t->cx - 1;
		while (n > 0 && col > 0) {
			while (col > 0 && !t->tabs[col])
				col--;
			n--;
		}
		move_cursor(t, t->cy, col);
	} else if (final_char == 'L') {
		scroll_region(t, t->cy, t->scroll_bottom, -MAX(1, params[0]));
	} else if (final_char == 'M') {
		scroll_region(t, t->cy, t->scroll_bottom, MAX(1, params[0]));
	} else if (final_char == '@') {
		insert_chars(t, MAX(1, params[0]));
	} else if (final_char == 'P') {
		delete_chars(t, MAX(1, params[0]));
	} else if (final_char == 'X') {
		fill_region(t, t->cx, t->cy, t->cx + MAX(1, params[0]) - 1, t->cy, ' ',
					0);
	} else if (final_char == 'S') {
		for (int n = MAX(1, params[0]); n > 0; n--)
			scroll_region(t, t->scroll_top, t->scroll_bottom, 1);
	} else if (final_char == 'T') {
		for (int n = MAX(1, params[0]); n > 0; n--)
			scroll_region(t, t->scroll_top, t->scroll_bottom, -1);
	} else if (final_char == 'g') {
		if (params[0] == 0 && t->cx < (int)sizeof(t->tabs))
			t->tabs[t->cx] = false;
		else if (params[0] == 3)
			memset(t->tabs, 0, sizeof(t->tabs));
	} else if (final_char == 'm') {
		sgr(t, nparams, params);
	} else if (final_char == 'r') {
		if (nparams == 2 && params[1] > params[0]) {
			t->scroll_top = MAX(params[0], 1) - 1;
			t->scroll_bottom = MIN(params[1], t->rows) - 1;
		} else if (params[0] == 0) {
			t->scroll_top = 0;
			t->scroll_bottom = t->rows - 1;
		}
		move_cursor(t, t->scroll_top, 0);
	} else if (final_char == 's') {
		save_cursor(t);
	} else if (final_char == 'u') {
		restore_cursor(t);
	} else if (final_char == 'c') {
		send_str(t, "\033[?6c");
	} else if (final_char == 'n') {
		if (params[0] == 5) {
			send_str(t, "\033[0n");
		} else if (params[0] == 6) {
			int top = t->origin_mode ? t->scroll_top : 0;
			char buf[32];
			snprintf(buf, sizeof(buf), "\033[%d;%dR", t->cy - top + 1,
					 t->cx + 1);
			send_str(t, buf);
		}
	}
}

static void feed_vt102(Terminal *t, uint8_t c) {
	if (t->state != TS_NORMAL && t->state != TS_READCHAR &&
		t->state != TS_GFXCMD) {
		if (c == 8 || c == 10 || c == 13) {
			process_text(t, c);
			return;
		}
	}

	switch (t->state) {
	case TS_NORMAL:
		if (c == 27)
			t->state = TS_WAITBRACKET;
		else
			process_text(t, c);
		break;

	case TS_WAITBRACKET:
		t->state = TS_NORMAL;
		switch (c) {
		case '[':
			t->start_char = 0;
			t->nparams = 1;
			t->params[0] = 0;
			t->state = TS_STARTCHAR;
			break;
		case 'g':
			t->state = TS_GFXCMD;
			t->gfx_need = -1;
			break;
		case '#':
			t->state = TS_HASH;
			break;
		case 27:
			put_char(t, 27);
			break;
		case 'c':
			reset_terminal(t);
			clear_screen(t);
			break;
		case '7':
			save_cursor(t);
			break;
		case '8':
			restore_cursor(t);
			break;
		case 'H':
			if (t->cx < (int)sizeof(t->tabs))
				t->tabs[t->cx] = true;
			break;
		case 'J':
			csi_dispatch(t, 0, 'J', 1, (uint8_t[]){0});
			break;
		case 'K':
			csi_dispatch(t, 0, 'K', 1, (uint8_t[]){0});
			break;
		case 'D':
			index_down(t);
			break;
		case 'E':
			t->cx = 0;
			index_down(t);
			break;
		case 'M':
			reverse_index(t);
			break;
		case 'Z':
			send_str(t, "\033[?6c");
			break;
		case '(':
		case ')':
		case '*':
		case '+':
			t->start_char = c;
			t->state = TS_READCHAR;
			break;
		}
		break;

	case TS_STARTCHAR:
	case TS_READPARAM:
		if (c >= '0' && c <= '9') {
			t->params[t->nparams - 1] =
				(uint8_t)(t->params[t->nparams - 1] * 10 + (c - '0'));
			t->state = TS_READPARAM;
		} else if (c == ';') {
			t->nparams++;
			if (t->nparams > 16) {
				t->nparams = 16;
				t->state = TS_NORMAL;
			} else {
				t->params[t->nparams - 1] = 0;
				t->state = TS_READPARAM;
			}
		} else if (t->state == TS_STARTCHAR && (c == '?' || c == '#')) {
			t->start_char = c;
			t->state = TS_READPARAM;
		} else {
			csi_dispatch(t, t->start_char, c, t->nparams, t->params);
			t->state = TS_NORMAL;
		}
		break;

	case TS_HASH:
		if (c == '8') {
			int top = t->origin_mode ? t->scroll_top : 0;
			int bot = t->origin_mode ? t->scroll_bottom : t->rows - 1;
			fill_region(t, 0, top, t->cols - 1, bot, 'E', 0);
		}
		t->state = TS_NORMAL;
		break;

	case TS_READCHAR:
		if (t->start_char == '(')
			t->g0 = (c == 'A') ? CS_UK : (c == '0' ? CS_GRAPHICS : CS_US);
		else if (t->start_char == ')')
			t->g1 = (c == 'A') ? CS_UK : (c == '0' ? CS_GRAPHICS : CS_US);
		t->state = TS_NORMAL;
		break;

	case TS_GFXCMD:
		/* VoidVDP bitmap protocol on the console. */
		if (t->gfx_need < 0) {
			t->gfx_cmd = c;
			t->gfx_index = 0;
			switch (c) {
			case 0x01:
			case 0x02:
				t->gfx_need = 1;
				break;
			case 0x03:
				t->gfx_need = 2;
				break;
			case 0x04:
				t->gfx_need = 5;
				break;
			case 0x05:
			case 0x06:
				t->gfx_need = 9;
				break;
			default:
				t->state = TS_NORMAL;
				break;
			}
		} else {
			t->gfx_params[t->gfx_index++] = c;
			if (t->gfx_index >= t->gfx_need) {
				gfx_exec(t);
				t->state = TS_NORMAL;
			}
		}
		break;
	}
}

static void feed_vt52(Terminal *t, uint8_t c) {
	static uint8_t row;
	static uint8_t start;

	switch (t->state) {
	case TS_NORMAL:
		if (c == 27) {
			t->state = TS_STARTCHAR;
		} else {
			process_text(t, c);
		}
		break;

	case TS_STARTCHAR:
		t->state = TS_NORMAL;
		switch (c) {
		case 'A':
			move_cursor(t, t->cy - 1, t->cx);
			break;
		case 'B':
			move_cursor(t, t->cy + 1, t->cx);
			break;
		case 'C':
			move_cursor(t, t->cy, t->cx + 1);
			break;
		case 'D':
			move_cursor(t, t->cy, t->cx - 1);
			break;
		case 'E':
			fill_region(t, 0, 0, t->cols - 1, t->rows - 1, ' ', 0);
			move_cursor(t, 0, 0);
			break;
		case 'H':
			move_cursor(t, 0, 0);
			break;
		case 'I':
			reverse_index(t);
			break;
		case 'J':
			fill_region(t, t->cx, t->cy, t->cols - 1, t->cy, ' ', 0);
			fill_region(t, 0, t->cy + 1, t->cols - 1, t->rows - 1, ' ', 0);
			break;
		case 'K':
			fill_region(t, t->cx, t->cy, t->cols - 1, t->cy, ' ', 0);
			break;
		case 'L':
			scroll_region(t, t->cy, t->rows - 1, -1);
			break;
		case 'M':
			scroll_region(t, t->cy, t->rows - 1, 1);
			break;
		case 'Y':
			start = c;
			row = 0;
			t->state = TS_READPARAM;
			break;
		case 'Z':
			send_str(t, "\033/K");
			break;
		case 'b':
		case 'c':
			start = c;
			t->state = TS_READPARAM;
			break;
		case 'j':
			save_cursor(t);
			break;
		case 'k':
			restore_cursor(t);
			break;
		case 'p':
			t->screen_inverted = true;
			break;
		case 'q':
			t->screen_inverted = false;
			break;
		case 'v':
			t->autowrap = true;
			break;
		case 'w':
			t->autowrap = false;
			break;
		case '<':
			reset_terminal(t);
			break;
		}
		break;

	case TS_READPARAM:
		if (start == 'Y') {
			if (row == 0) {
				row = c;
			} else {
				if (row >= 32 && c >= 32)
					move_cursor(t, row - 32, c - 32);
				t->state = TS_NORMAL;
			}
		} else if (start == 'b' && c >= 32) {
			t->pen.fg = (c - 32) & 15;
		} else if (start == 'c' && c >= 32) {
			t->pen.bg = (c - 32) & 15;
		}
		break;
	}
}

Terminal *terminal_create(int cols, int rows) {
	Terminal *t = calloc(1, sizeof(*t));
	if (!t)
		return NULL;
	t->cols = cols;
	t->rows = rows;
	t->cells = calloc((size_t)cols * rows, sizeof(TermCell));
	if (!t->cells) {
		free(t);
		return NULL;
	}
	reset_terminal(t);
	clear_screen(t);
	return t;
}

void terminal_destroy(Terminal *t) {
	if (!t)
		return;
	free(t->cells);
	free(t);
}

int terminal_cols(const Terminal *t) { return t->cols; }
int terminal_rows(const Terminal *t) { return t->rows; }
const TermCell *terminal_cells(const Terminal *t) { return t->cells; }
int terminal_cursor_x(const Terminal *t) { return t->cx; }
int terminal_cursor_y(const Terminal *t) { return t->cy; }
bool terminal_cursor_visible(const Terminal *t) { return t->cursor_visible; }
bool terminal_screen_inverted(const Terminal *t) { return t->screen_inverted; }
bool terminal_vt52(const Terminal *t) { return t->vt52_mode; }
unsigned terminal_bell_count(const Terminal *t) { return t->bell_count; }

bool terminal_gfx_active(const Terminal *t) { return t->gfx_active; }
const uint8_t *terminal_gfx_bits(const Terminal *t) { return t->gfx_buf; }
uint8_t terminal_gfx_bg(const Terminal *t) { return t->gfx_bg; }

void terminal_gfx_cell_color(const Terminal *t, int col, int row, uint8_t *fg,
							 uint8_t *bg) {
	if (col < 0 || col >= TERM_GFX_STRIDE || row < 0 || row >= TERM_GFX_ROWS) {
		*fg = t->gfx_fg;
		*bg = t->gfx_bg;
		return;
	}
	size_t idx = (size_t)row * TERM_GFX_STRIDE + col;
	*fg = t->gfx_cell_fg[idx];
	*bg = t->gfx_cell_bg[idx];
}

void terminal_feed(Terminal *t, const uint8_t *data, size_t len) {
	for (size_t i = 0; i < len; i++) {
		if (t->vt52_mode)
			feed_vt52(t, data[i]);
		else
			feed_vt102(t, data[i]);
	}
}

const uint8_t *terminal_output(const Terminal *t, size_t *len) {
	*len = t->out_len;
	return t->out;
}

void terminal_output_clear(Terminal *t) { t->out_len = 0; }

void terminal_send(Terminal *t, const void *data, size_t len) {
	out_append(t, data, len);
}
