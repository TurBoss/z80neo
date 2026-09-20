#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TERM_DEFAULT_COLS 80
#define TERM_DEFAULT_ROWS 24

#define TERM_ATTR_BOLD 0x01
#define TERM_ATTR_UNDERLINE 0x02
#define TERM_ATTR_BLINK 0x04
#define TERM_ATTR_INVERSE 0x08

#define TERM_OUT_MAX 4096

/* VoidVDP bitmap layer (the VDP is 640x256, two colours per 8x16 cell). */
#define TERM_GFX_WIDTH 640
#define TERM_GFX_HEIGHT 256
#define TERM_GFX_STRIDE (TERM_GFX_WIDTH / 8)
#define TERM_GFX_CELL_H 16
#define TERM_GFX_ROWS (TERM_GFX_HEIGHT / TERM_GFX_CELL_H)

typedef struct {
	uint32_t ch;
	uint8_t fg;
	uint8_t bg;
	uint8_t attr;
} TermCell;

typedef struct Terminal Terminal;

Terminal *terminal_create(int cols, int rows);
void terminal_destroy(Terminal *t);

int terminal_cols(const Terminal *t);
int terminal_rows(const Terminal *t);
const TermCell *terminal_cells(const Terminal *t);
int terminal_cursor_x(const Terminal *t);
int terminal_cursor_y(const Terminal *t);
bool terminal_cursor_visible(const Terminal *t);
bool terminal_screen_inverted(const Terminal *t);
bool terminal_vt52(const Terminal *t);

/* Number of BEL (0x07) characters received so far; monotonic. */
unsigned terminal_bell_count(const Terminal *t);

/* VoidVDP bitmap layer.  When active the client renders this instead of the
   text grid.  `bits` is 1bpp, TERM_GFX_STRIDE bytes per row. */
bool terminal_gfx_active(const Terminal *t);
const uint8_t *terminal_gfx_bits(const Terminal *t);
void terminal_gfx_cell_color(const Terminal *t, int col, int row, uint8_t *fg,
							 uint8_t *bg);
uint8_t terminal_gfx_bg(const Terminal *t);

void terminal_feed(Terminal *t, const uint8_t *data, size_t len);

/* Bytes the emulator wants to send back to the host (reports, answerback). */
const uint8_t *terminal_output(const Terminal *t, size_t *len);
void terminal_output_clear(Terminal *t);

/* Queue raw bytes for the host (keyboard/paste input). */
void terminal_send(Terminal *t, const void *data, size_t len);

#endif
