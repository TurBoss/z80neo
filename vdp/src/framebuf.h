// -----------------------------------------------------------------------------
// VoidVDP - A versatile serial terminal (fork of VersaTerm)
// Copyright (C) 2022 David Hansel
// Copyright (C) 2026 turboss
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
// -----------------------------------------------------------------------------

#ifndef FRAMEBUF_H
#define FRAMEBUF_H

#include "pico/stdlib.h"
#include "font.h"

#define FRAME_WIDTH  640
#define FRAME_HEIGHT 480
#define MAX_COLS     ((uint32_t) (FRAME_WIDTH / FONT_CHAR_WIDTH))
#define MAX_ROWS     ((uint32_t) (FRAME_HEIGHT / font_get_char_height()))

#define ATTR_UNDERLINE 0x01
#define ATTR_BLINK     0x02
#define ATTR_BOLD      0x04
#define ATTR_INVERSE   0x08

#define ROW_ATTR_DBL_WIDTH       0x01
#define ROW_ATTR_DBL_HEIGHT_TOP  0x02
#define ROW_ATTR_DBL_HEIGHT_BOT  0x04

void framebuf_init(bool forceDVI);
void framebuf_apply_settings();
bool framebuf_is_dvi();

void framebuf_set_char(uint8_t column, uint8_t row, uint8_t character);
uint8_t framebuf_get_char(uint8_t column, uint8_t row);

void    framebuf_set_attr(uint8_t column, uint8_t row, uint8_t a);
uint8_t framebuf_get_attr(uint8_t column, uint8_t row);

void    framebuf_set_row_attr(uint8_t row, uint8_t a);
uint8_t framebuf_get_row_attr(uint8_t row);

void framebuf_set_color(uint8_t column, uint8_t row, uint8_t foreground, uint8_t background);
void framebuf_set_fullcolor(uint8_t x, uint8_t y, uint8_t fg, uint8_t bg);

void framebuf_fill_screen(char character, uint8_t fg, uint8_t bg);
void framebuf_fill_region(uint8_t col_start, uint8_t row_start, uint8_t col_end, uint8_t row_end, char character, uint8_t fg, uint8_t bg);

void framebuf_scroll_screen(int8_t n, uint8_t fg, uint8_t bg);
void framebuf_scroll_region(uint8_t row_start, uint8_t row_end, int8_t n, uint8_t fg, uint8_t bg);

void framebuf_insert(uint8_t x, uint8_t y, uint8_t n, uint8_t fg, uint8_t bg);
void framebuf_delete(uint8_t x, uint8_t y, uint8_t n, uint8_t fg, uint8_t bg);

uint8_t framebuf_get_nrows();
uint8_t framebuf_get_ncols(int row);

void framebuf_set_scroll_delay(uint16_t ms);
void framebuf_set_screen_size(uint8_t ncols, uint8_t nrows);
void framebuf_set_screen_inverted(bool invert);
void framebuf_flash_screen(uint8_t color, uint8_t nframes);

// ---- Monochrome bitmap graphics layer -------------------------------------
// A 1bpp bitmap drawn on top of the character cells, two colours (fg/bg) per
// character cell.  Only the DVI renderer implements it; on VGA the calls are
// no-ops.  Pixels are addressed with bit (x&7) of byte (x>>3), matching the
// font bit order used by the TMDS encoder (bit 0 = leftmost pixel).
#define GFX_WIDTH   640
#define GFX_HEIGHT  256
#define GFX_STRIDE  (GFX_WIDTH / 8)
#define GFX_YOFF    ((FRAME_HEIGHT - GFX_HEIGHT) / 2)

extern bool    framebuf_gfx_active;             // renderer reads this
extern uint8_t framebuf_gfx_buf[GFX_STRIDE * GFX_HEIGHT];

bool framebuf_gfx_enabled();
bool framebuf_gfx_enable(bool on);
void framebuf_gfx_clear();
void framebuf_gfx_set_colors(uint8_t fg, uint8_t bg);
void framebuf_gfx_get_colors(uint8_t *fg, uint8_t *bg);
void framebuf_gfx_pixel(int x, int y, bool on);
bool framebuf_gfx_get_pixel(int x, int y);
void framebuf_gfx_line(int x0, int y0, int x1, int y1, bool on);
void framebuf_gfx_fill(int x, int y, int w, int h, bool on);
void framebuf_gfx_draw_char(int col, int row, uint8_t ch);

#endif
