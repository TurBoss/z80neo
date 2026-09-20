// screensaver.c
// Z80NEO Firmware — 15-puzzle OLED screensaver
// TurBoss 2026

#include <string.h>

#include <pico/rand.h>
#include <pico/time.h>

#include "screensaver.h"
#include "ssd1306_i2c.h"

#define SS_COLS 4
#define SS_ROWS 4
#define SS_CELLS (SS_COLS * SS_ROWS)
#define SS_TILE_W (SSD1306_WIDTH / SS_COLS)
#define SS_TILE_H (SSD1306_HEIGHT / SS_ROWS)
#define SS_TILE_PAGES (SS_TILE_H / 8)

#define SS_MAX_MOVES 64
#define SS_SHUFFLE_MOVES 40

#define SS_REVEAL_US (2500 * 1000)
#define SS_HOLD_US (800 * 1000)
#define SS_SHUFFLE_US (90 * 1000)
#define SS_SOLVE_US (170 * 1000)

enum { SS_REVEAL = 0, SS_SHUFFLE, SS_HOLD_A, SS_SOLVE, SS_HOLD_B };

static bool ss_on;
static const uint8_t *ss_img;
static uint8_t ss_board[SS_CELLS];
static uint8_t ss_blank;
static uint8_t ss_prev_blank;
static uint8_t ss_mv_a[SS_MAX_MOVES];
static uint8_t ss_mv_b[SS_MAX_MOVES];
static uint8_t ss_mv_count;
static uint8_t ss_mv_idx;
static int ss_state;
static uint64_t ss_next;
static bool ss_fill;

static void ss_solved(void) {
	for (int i = 0; i < SS_CELLS - 1; i++)
		ss_board[i] = i + 1;
	ss_board[SS_CELLS - 1] = 0;
	ss_blank = SS_CELLS - 1;
	ss_prev_blank = 0xFF;
}

static void ss_blit(uint8_t *dst, int src_cell, int dst_cell) {
	int scx = src_cell % SS_COLS, scy = src_cell / SS_COLS;
	int dcx = dst_cell % SS_COLS, dcy = dst_cell / SS_COLS;
	const uint8_t *s =
		ss_img + (scy * SS_TILE_PAGES) * SSD1306_WIDTH + scx * SS_TILE_W;
	uint8_t *d = dst + (dcy * SS_TILE_PAGES) * SSD1306_WIDTH + dcx * SS_TILE_W;

	for (int p = 0; p < SS_TILE_PAGES; p++)
		memcpy(d + p * SSD1306_WIDTH, s + p * SSD1306_WIDTH, SS_TILE_W);
}

static void ss_render(uint8_t *dst) {
	memset(dst, 0, SSD1306_BUF_LEN);
	for (int cell = 0; cell < SS_CELLS; cell++) {
		uint8_t v = ss_board[cell];
		if (v == 0) {
			if (ss_fill)
				ss_blit(dst, cell, cell);
			continue;
		}
		ss_blit(dst, v - 1, cell);
	}
}

static int ss_neighbors(int cell, uint8_t *out) {
	int n = 0;
	int col = cell % SS_COLS;
	if (cell >= SS_COLS)
		out[n++] = cell - SS_COLS;
	if (cell < SS_CELLS - SS_COLS)
		out[n++] = cell + SS_COLS;
	if (col > 0)
		out[n++] = cell - 1;
	if (col < SS_COLS - 1)
		out[n++] = cell + 1;
	return n;
}

static void ss_move(uint8_t target) {
	uint8_t t = ss_board[target];
	ss_board[target] = ss_board[ss_blank];
	ss_board[ss_blank] = t;
	ss_prev_blank = ss_blank;
	ss_blank = target;
}

static void ss_random_move(void) {
	uint8_t nb[4];
	int n = ss_neighbors(ss_blank, nb);
	int pick;
	do {
		pick = (int)(get_rand_32() % (uint32_t)n);
	} while (nb[pick] == ss_prev_blank && n > 1);

	if (ss_mv_count < SS_MAX_MOVES) {
		ss_mv_a[ss_mv_count] = ss_blank;
		ss_mv_b[ss_mv_count] = nb[pick];
		ss_mv_count++;
	}
	ss_move(nb[pick]);
}

void screensaver_begin(const uint8_t *img) {
	ss_img = img;
	ss_on = true;
	ss_state = SS_REVEAL;
	ss_fill = true;
	ss_mv_count = 0;
	ss_mv_idx = 0;
	ss_solved();
	ss_next = time_us_64() + SS_REVEAL_US;
}

void screensaver_stop(void) { ss_on = false; }

bool screensaver_active(void) { return ss_on; }

bool screensaver_step(uint8_t *dst) {
	if (!ss_on)
		return false;

	uint64_t now = time_us_64();

	switch (ss_state) {
	case SS_REVEAL:
		ss_fill = true;
		ss_render(dst);
		ss_state = SS_SHUFFLE;
		ss_mv_count = 0;
		ss_mv_idx = 0;
		ss_prev_blank = 0xFF;
		ss_next = now + SS_REVEAL_US;
		return true;

	case SS_SHUFFLE:
		if (now < ss_next)
			return false;
		ss_fill = false;
		if (ss_mv_count < SS_SHUFFLE_MOVES) {
			ss_random_move();
			ss_next = now + SS_SHUFFLE_US;
		} else {
			ss_state = SS_HOLD_A;
			ss_next = now + SS_HOLD_US;
		}
		ss_render(dst);
		return true;

	case SS_HOLD_A:
		if (now < ss_next)
			return false;
		ss_state = SS_SOLVE;
		ss_mv_idx = ss_mv_count;
		ss_next = now;
		return false;

	case SS_SOLVE:
		if (now < ss_next)
			return false;
		if (ss_mv_idx > 0) {
			ss_mv_idx--;
			ss_move(ss_mv_a[ss_mv_idx]);
			ss_next = now + SS_SOLVE_US;
		} else {
			ss_state = SS_HOLD_B;
			ss_fill = true;
			ss_next = now + SS_REVEAL_US;
		}
		ss_render(dst);
		return true;

	case SS_HOLD_B:
		if (now < ss_next)
			return false;
		ss_state = SS_SHUFFLE;
		ss_mv_count = 0;
		ss_mv_idx = 0;
		ss_prev_blank = 0xFF;
		ss_next = now;
		return false;
	}

	return false;
}
