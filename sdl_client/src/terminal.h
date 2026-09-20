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

/* VoidVDP true-colour bitmap layer: 640x256, one 4-bit palette index per pixel
   (stored in a byte). */
#define TERM_GFX_WIDTH 640
#define TERM_GFX_HEIGHT 256

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
   text grid.  `pixels` is one palette index per pixel, row-major, 640 wide. */
bool terminal_gfx_active(const Terminal *t);
const uint8_t *terminal_gfx_pixels(const Terminal *t);
uint8_t terminal_gfx_bg(const Terminal *t);

void terminal_feed(Terminal *t, const uint8_t *data, size_t len);

/* Bytes the emulator wants to send back to the host (reports, answerback). */
const uint8_t *terminal_output(const Terminal *t, size_t *len);
void terminal_output_clear(Terminal *t);

/* Queue raw bytes for the host (keyboard/paste input). */
void terminal_send(Terminal *t, const void *data, size_t len);

#endif
