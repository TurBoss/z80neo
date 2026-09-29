// screensaver.c
// Z80NEO Firmware — OLED screensaver demos
// TurBoss 2026

#include <stdlib.h>
#include <string.h>

#include <pico/rand.h>
#include <pico/time.h>

#include "screensaver.h"
#include "ssd1306_i2c.h"

// ===========================================================================
// Shared state / settings
// ===========================================================================

static bool ss_on;
static int ss_mode = SS_MODE_PUZZLE;
static bool ss_enabled = true;
static uint32_t ss_idle_s = 30;
static const uint8_t *ss_img;
static uint64_t ss_next;

static const char *const ss_mode_names[SS_MODE_COUNT] = {
	"Puzzle", "Snake", "Starfield", "Life", "Bounce",
};

int screensaver_mode_count(void) { return SS_MODE_COUNT; }

const char *screensaver_mode_name(int mode) {
	if (mode < 0 || mode >= SS_MODE_COUNT)
		return "?";
	return ss_mode_names[mode];
}

void screensaver_set_mode(int mode) {
	if (mode < 0)
		mode = 0;
	if (mode >= SS_MODE_COUNT)
		mode = SS_MODE_COUNT - 1;
	ss_mode = mode;
}

int screensaver_mode(void) { return ss_mode; }

bool screensaver_enabled(void) { return ss_enabled; }

void screensaver_set_enabled(bool on) { ss_enabled = on; }

void screensaver_set_idle_seconds(uint32_t s) {
	if (s < 5)
		s = 5;
	if (s > 3600)
		s = 3600;
	ss_idle_s = s;
}

uint32_t screensaver_idle_seconds(void) { return ss_idle_s; }

uint64_t screensaver_idle_us(void) { return (uint64_t)ss_idle_s * 1000000ULL; }

// ===========================================================================
// 1bpp drawing helpers (SSD1306 page format, 8 pages x 128 bytes)
// ===========================================================================

static void ss_px(uint8_t *dst, int x, int y, bool on) {
	if (x < 0 || x >= SSD1306_WIDTH || y < 0 || y >= SSD1306_HEIGHT)
		return;
	uint8_t *p = dst + (y >> 3) * SSD1306_WIDTH + x;
	if (on)
		*p |= (uint8_t)(1u << (y & 7));
	else
		*p &= (uint8_t)~(1u << (y & 7));
}

static void ss_fill_rect(uint8_t *dst, int x, int y, int w, int h, bool on) {
	for (int j = 0; j < h; j++)
		for (int i = 0; i < w; i++)
			ss_px(dst, x + i, y + j, on);
}

static void ss_rect(uint8_t *dst, int x, int y, int w, int h, bool on) {
	for (int i = 0; i < w; i++) {
		ss_px(dst, x + i, y, on);
		ss_px(dst, x + i, y + h - 1, on);
	}
	for (int j = 0; j < h; j++) {
		ss_px(dst, x, y + j, on);
		ss_px(dst, x + w - 1, y + j, on);
	}
}

static void ss_disc(uint8_t *dst, int cx, int cy, int r, bool on) {
	for (int dy = -r; dy <= r; dy++)
		for (int dx = -r; dx <= r; dx++)
			if (dx * dx + dy * dy <= r * r)
				ss_px(dst, cx + dx, cy + dy, on);
}

// ===========================================================================
// Puzzle demo — 15-puzzle built from the logo image
// ===========================================================================

#define PZ_COLS 4
#define PZ_ROWS 4
#define PZ_CELLS (PZ_COLS * PZ_ROWS)
#define PZ_TILE_W (SSD1306_WIDTH / PZ_COLS)
#define PZ_TILE_H (SSD1306_HEIGHT / PZ_ROWS)
#define PZ_TILE_PAGES (PZ_TILE_H / 8)

#define PZ_MAX_MOVES 64
#define PZ_SHUFFLE_MOVES 40

#define PZ_REVEAL_US (2500 * 1000)
#define PZ_HOLD_US (800 * 1000)
#define PZ_SHUFFLE_US (90 * 1000)
#define PZ_SOLVE_US (170 * 1000)

enum { PZ_REVEAL = 0, PZ_SHUFFLE, PZ_HOLD_A, PZ_SOLVE, PZ_HOLD_B };

static uint8_t pz_board[PZ_CELLS];
static uint8_t pz_blank;
static uint8_t pz_prev_blank;
static uint8_t pz_mv_a[PZ_MAX_MOVES];
static uint8_t pz_mv_b[PZ_MAX_MOVES];
static uint8_t pz_mv_count;
static uint8_t pz_mv_idx;
static int pz_state;
static bool pz_fill;

static void pz_solved(void) {
	for (int i = 0; i < PZ_CELLS - 1; i++)
		pz_board[i] = i + 1;
	pz_board[PZ_CELLS - 1] = 0;
	pz_blank = PZ_CELLS - 1;
	pz_prev_blank = 0xFF;
}

static void pz_blit(uint8_t *dst, int src_cell, int dst_cell) {
	int scx = src_cell % PZ_COLS, scy = src_cell / PZ_COLS;
	int dcx = dst_cell % PZ_COLS, dcy = dst_cell / PZ_COLS;
	const uint8_t *s =
		ss_img + (scy * PZ_TILE_PAGES) * SSD1306_WIDTH + scx * PZ_TILE_W;
	uint8_t *d = dst + (dcy * PZ_TILE_PAGES) * SSD1306_WIDTH + dcx * PZ_TILE_W;

	for (int p = 0; p < PZ_TILE_PAGES; p++)
		memcpy(d + p * SSD1306_WIDTH, s + p * SSD1306_WIDTH, PZ_TILE_W);
}

static void pz_render(uint8_t *dst) {
	memset(dst, 0, SSD1306_BUF_LEN);
	for (int cell = 0; cell < PZ_CELLS; cell++) {
		uint8_t v = pz_board[cell];
		if (v == 0) {
			if (pz_fill)
				pz_blit(dst, cell, cell);
			continue;
		}
		pz_blit(dst, v - 1, cell);
	}
}

static int pz_neighbors(int cell, uint8_t *out) {
	int n = 0;
	int col = cell % PZ_COLS;
	if (cell >= PZ_COLS)
		out[n++] = cell - PZ_COLS;
	if (cell < PZ_CELLS - PZ_COLS)
		out[n++] = cell + PZ_COLS;
	if (col > 0)
		out[n++] = cell - 1;
	if (col < PZ_COLS - 1)
		out[n++] = cell + 1;
	return n;
}

static void pz_move(uint8_t target) {
	uint8_t t = pz_board[target];
	pz_board[target] = pz_board[pz_blank];
	pz_board[pz_blank] = t;
	pz_prev_blank = pz_blank;
	pz_blank = target;
}

static void pz_random_move(void) {
	uint8_t nb[4];
	int n = pz_neighbors(pz_blank, nb);
	int pick;
	do {
		pick = (int)(get_rand_32() % (uint32_t)n);
	} while (nb[pick] == pz_prev_blank && n > 1);

	if (pz_mv_count < PZ_MAX_MOVES) {
		pz_mv_a[pz_mv_count] = pz_blank;
		pz_mv_b[pz_mv_count] = nb[pick];
		pz_mv_count++;
	}
	pz_move(nb[pick]);
}

static void pz_begin(void) {
	pz_state = PZ_REVEAL;
	pz_fill = true;
	pz_mv_count = 0;
	pz_mv_idx = 0;
	pz_solved();
	ss_next = time_us_64() + PZ_REVEAL_US;
}

static bool pz_step(uint8_t *dst, uint64_t now) {
	switch (pz_state) {
	case PZ_REVEAL:
		pz_fill = true;
		pz_render(dst);
		pz_state = PZ_SHUFFLE;
		pz_mv_count = 0;
		pz_mv_idx = 0;
		pz_prev_blank = 0xFF;
		ss_next = now + PZ_REVEAL_US;
		return true;

	case PZ_SHUFFLE:
		if (now < ss_next)
			return false;
		pz_fill = false;
		if (pz_mv_count < PZ_SHUFFLE_MOVES) {
			pz_random_move();
			ss_next = now + PZ_SHUFFLE_US;
		} else {
			pz_state = PZ_HOLD_A;
			ss_next = now + PZ_HOLD_US;
		}
		pz_render(dst);
		return true;

	case PZ_HOLD_A:
		if (now < ss_next)
			return false;
		pz_state = PZ_SOLVE;
		pz_mv_idx = pz_mv_count;
		ss_next = now;
		return false;

	case PZ_SOLVE:
		if (now < ss_next)
			return false;
		if (pz_mv_idx > 0) {
			pz_mv_idx--;
			pz_move(pz_mv_a[pz_mv_idx]);
			ss_next = now + PZ_SOLVE_US;
		} else {
			pz_state = PZ_HOLD_B;
			pz_fill = true;
			ss_next = now + PZ_REVEAL_US;
		}
		pz_render(dst);
		return true;

	case PZ_HOLD_B:
		if (now < ss_next)
			return false;
		pz_state = PZ_SHUFFLE;
		pz_mv_count = 0;
		pz_mv_idx = 0;
		pz_prev_blank = 0xFF;
		ss_next = now;
		return false;
	}

	return false;
}

// ===========================================================================
// Snake demo — self-playing snake chasing a food dot
// ===========================================================================

#define SN_COLS 32
#define SN_ROWS 16
#define SN_CELL 4
#define SN_MAX 160

#define SN_STEP_US (70 * 1000)

static int8_t sn_x[SN_MAX], sn_y[SN_MAX]; // body, [0] = head
static int sn_len;
static int sn_dir;
static int sn_fx, sn_fy;

static bool sn_occupied(int x, int y, bool ignore_tail) {
	int last = sn_len - (ignore_tail ? 1 : 0);
	for (int i = 0; i < last; i++)
		if (sn_x[i] == x && sn_y[i] == y)
			return true;
	return false;
}

static void sn_spawn_food(void) {
	for (int try = 0; try < 1000; try++) {
		int x = (int)(get_rand_32() % SN_COLS);
		int y = (int)(get_rand_32() % SN_ROWS);
		if (!sn_occupied(x, y, false)) {
			sn_fx = x;
			sn_fy = y;
			return;
		}
	}
}

static void sn_reset(void) {
	sn_len = 4;
	sn_dir = 1;
	int sx = SN_COLS / 2, sy = SN_ROWS / 2;
	for (int i = 0; i < sn_len; i++) {
		sn_x[i] = (int8_t)(sx - i);
		sn_y[i] = (int8_t)sy;
	}
	sn_spawn_food();
}

static void sn_tick(void) {
	int hx = sn_x[0], hy = sn_y[0];
	static const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
	int rev = sn_dir ^ 1; // opposites: 0<->1, 2<->3
	int best = -1, best_score = 1 << 30;

	for (int d = 0; d < 4; d++) {
		int nx = hx + dirs[d][0], ny = hy + dirs[d][1];
		if (nx < 0 || nx >= SN_COLS || ny < 0 || ny >= SN_ROWS)
			continue;
		if (sn_occupied(nx, ny, true))
			continue;
		int score = abs(sn_fx - nx) + abs(sn_fy - ny);
		if (d == rev)
			score += 50;
		if (score < best_score) {
			best_score = score;
			best = d;
		}
	}

	if (best < 0) {
		sn_reset();
		return;
	}

	int nx = hx + dirs[best][0], ny = hy + dirs[best][1];
	bool eat = (nx == sn_fx && ny == sn_fy);

	if (eat && sn_len < SN_MAX) {
		for (int i = sn_len; i > 0; i--) {
			sn_x[i] = sn_x[i - 1];
			sn_y[i] = sn_y[i - 1];
		}
		sn_len++;
	} else {
		for (int i = sn_len - 1; i > 0; i--) {
			sn_x[i] = sn_x[i - 1];
			sn_y[i] = sn_y[i - 1];
		}
	}
	sn_x[0] = (int8_t)nx;
	sn_y[0] = (int8_t)ny;
	sn_dir = best;

	if (eat) {
		if (sn_len >= SN_MAX) {
			sn_reset();
			return;
		}
		sn_spawn_food();
	}
}

static void sn_render(uint8_t *dst) {
	memset(dst, 0, SSD1306_BUF_LEN);
	ss_fill_rect(dst, sn_fx * SN_CELL + 1, sn_fy * SN_CELL + 1, 2, 2, true);
	for (int i = sn_len - 1; i >= 0; i--)
		ss_fill_rect(dst, sn_x[i] * SN_CELL, sn_y[i] * SN_CELL, SN_CELL,
					 SN_CELL, true);
	// Hollow the head so its position reads as the front of the snake.
	ss_fill_rect(dst, sn_x[0] * SN_CELL + 1, sn_y[0] * SN_CELL + 1, 2, 2,
				 false);
}

// ===========================================================================
// Starfield demo — stars flying toward the viewer
// ===========================================================================

#define ST_COUNT 72
#define ST_STEP_US (33 * 1000)

static int8_t st_x[ST_COUNT], st_y[ST_COUNT]; // -64..64
static uint8_t st_z[ST_COUNT];				  // 1..64

static void st_respawn(int i) {
	st_x[i] = (int8_t)((int)(get_rand_32() % 129) - 64);
	st_y[i] = (int8_t)((int)(get_rand_32() % 129) - 64);
	st_z[i] = 64;
}

static void st_reset(void) {
	for (int i = 0; i < ST_COUNT; i++)
		st_respawn(i);
}

static void st_tick(void) {
	for (int i = 0; i < ST_COUNT; i++) {
		int z = (int)st_z[i] - 3;
		if (z <= 0)
			st_respawn(i);
		else
			st_z[i] = (uint8_t)z;
	}
}

static void st_render(uint8_t *dst) {
	memset(dst, 0, SSD1306_BUF_LEN);
	for (int i = 0; i < ST_COUNT; i++) {
		int z = st_z[i];
		int px = 64 + (st_x[i] * 64) / z;
		int py = 32 + (st_y[i] * 32) / z;
		if (px < 0 || px >= SSD1306_WIDTH || py < 0 || py >= SSD1306_HEIGHT)
			continue;
		ss_px(dst, px, py, true);
		if (z < 24) { // brighten the nearest stars
			ss_px(dst, px + 1, py, true);
			ss_px(dst, px, py + 1, true);
		}
	}
}

// ===========================================================================
// Life demo — Conway's Game of Life on a 64x32 grid (2x2 px cells)
// ===========================================================================

#define LF_W 64
#define LF_H 32
#define LF_CELL 2
#define LF_STEP_US (90 * 1000)

static uint8_t lf_a[LF_W * LF_H];
static uint8_t lf_b[LF_W * LF_H];

static uint8_t lf_get(const uint8_t *g, int x, int y) {
	x = (x + LF_W) % LF_W;
	y = (y + LF_H) % LF_H;
	return g[y * LF_W + x];
}

static void lf_reset(void) {
	for (int i = 0; i < LF_W * LF_H; i++)
		lf_a[i] = (uint8_t)((get_rand_32() % 100) < 30);
}

static void lf_tick(void) {
	for (int y = 0; y < LF_H; y++) {
		for (int x = 0; x < LF_W; x++) {
			int n = lf_get(lf_a, x - 1, y - 1) + lf_get(lf_a, x, y - 1) +
					lf_get(lf_a, x + 1, y - 1) + lf_get(lf_a, x - 1, y) +
					lf_get(lf_a, x + 1, y) + lf_get(lf_a, x - 1, y + 1) +
					lf_get(lf_a, x, y + 1) + lf_get(lf_a, x + 1, y + 1);
			uint8_t c = lf_a[y * LF_W + x];
			lf_b[y * LF_W + x] = (uint8_t)(c ? (n == 2 || n == 3) : (n == 3));
		}
	}
	memcpy(lf_a, lf_b, sizeof(lf_a));
}

static void lf_render(uint8_t *dst) {
	memset(dst, 0, SSD1306_BUF_LEN);
	for (int y = 0; y < LF_H; y++)
		for (int x = 0; x < LF_W; x++)
			if (lf_a[y * LF_W + x])
				ss_fill_rect(dst, x * LF_CELL, y * LF_CELL, LF_CELL, LF_CELL,
							 true);
}

// ===========================================================================
// Bounce demo — ball bouncing off the screen edges with a fading trail
// ===========================================================================

#define BN_R 5
#define BN_TRAIL 12
#define BN_STEP_US (25 * 1000)

static int bn_x, bn_y, bn_vx, bn_vy;
static int8_t bn_tx[BN_TRAIL], bn_ty[BN_TRAIL];
static int bn_ti;

static void bn_reset(void) {
	bn_x = 20 + (int)(get_rand_32() % 80);
	bn_y = 10 + (int)(get_rand_32() % 40);
	bn_vx = (get_rand_32() & 1) ? 2 : -2;
	bn_vy = (get_rand_32() & 1) ? 2 : -2;
	bn_ti = 0;
	for (int i = 0; i < BN_TRAIL; i++) {
		bn_tx[i] = -1;
		bn_ty[i] = -1;
	}
}

static void bn_tick(void) {
	bn_x += bn_vx;
	bn_y += bn_vy;
	if (bn_x < BN_R) {
		bn_x = BN_R;
		bn_vx = -bn_vx;
	}
	if (bn_x > SSD1306_WIDTH - 1 - BN_R) {
		bn_x = SSD1306_WIDTH - 1 - BN_R;
		bn_vx = -bn_vx;
	}
	if (bn_y < BN_R) {
		bn_y = BN_R;
		bn_vy = -bn_vy;
	}
	if (bn_y > SSD1306_HEIGHT - 1 - BN_R) {
		bn_y = SSD1306_HEIGHT - 1 - BN_R;
		bn_vy = -bn_vy;
	}
	bn_tx[bn_ti] = (int8_t)bn_x;
	bn_ty[bn_ti] = (int8_t)bn_y;
	bn_ti = (bn_ti + 1) % BN_TRAIL;
}

static void bn_render(uint8_t *dst) {
	memset(dst, 0, SSD1306_BUF_LEN);
	for (int i = 0; i < BN_TRAIL; i++) {
		if (bn_tx[i] < 0)
			continue;
		int age = (bn_ti - i + BN_TRAIL) % BN_TRAIL;
		if (age > 0 && age <= 6)
			ss_px(dst, bn_tx[i], bn_ty[i], true);
	}
	ss_disc(dst, bn_x, bn_y, BN_R, true);
}

// ===========================================================================
// Public entry points
// ===========================================================================

void screensaver_begin(const uint8_t *img) {
	ss_img = img;
	ss_on = true;
	ss_next = time_us_64();

	switch (ss_mode) {
	case SS_MODE_SNAKE:
		sn_reset();
		break;
	case SS_MODE_STARFIELD:
		st_reset();
		break;
	case SS_MODE_LIFE:
		lf_reset();
		break;
	case SS_MODE_BOUNCE:
		bn_reset();
		break;
	default:
		pz_begin();
		break;
	}
}

void screensaver_stop(void) { ss_on = false; }

bool screensaver_active(void) { return ss_on; }

bool screensaver_step(uint8_t *dst) {
	if (!ss_on)
		return false;

	uint64_t now = time_us_64();

	switch (ss_mode) {
	case SS_MODE_SNAKE:
		if (now < ss_next)
			return false;
		ss_next = now + SN_STEP_US;
		sn_tick();
		sn_render(dst);
		return true;

	case SS_MODE_STARFIELD:
		if (now < ss_next)
			return false;
		ss_next = now + ST_STEP_US;
		st_tick();
		st_render(dst);
		return true;

	case SS_MODE_LIFE:
		if (now < ss_next)
			return false;
		ss_next = now + LF_STEP_US;
		lf_tick();
		lf_render(dst);
		return true;

	case SS_MODE_BOUNCE:
		if (now < ss_next)
			return false;
		ss_next = now + BN_STEP_US;
		bn_tick();
		bn_render(dst);
		return true;

	default:
		return pz_step(dst, now);
	}
}
