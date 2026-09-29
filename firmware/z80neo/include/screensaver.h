#ifndef SCREENSAVER_H
#define SCREENSAVER_H

#include <stdbool.h>
#include <stdint.h>

// OLED screensaver: a small collection of demos that animate the idle SSD1306
// while the Z80 keeps running.  The active demo and idle timeout are
// user-selectable from the firmware menu and may be seeded from Z80NEO.INI
// (SCREENSAVER / SS_MODE / SS_IDLE).

// Demo ids.  Keep screensaver_mode_name() in step with this list.
enum {
	SS_MODE_PUZZLE = 0, // 15-puzzle built from the logo image
	SS_MODE_SNAKE,		// self-playing snake
	SS_MODE_STARFIELD,	// 3D star tunnel
	SS_MODE_LIFE,		// Conway's Game of Life
	SS_MODE_BOUNCE,		// bouncing ball with a trail
	SS_MODE_COUNT
};

int screensaver_mode_count(void);
const char *screensaver_mode_name(int mode);

// Select the demo (clamped to the valid range).  Takes effect on the next
// screensaver_begin().
void screensaver_set_mode(int mode);
int screensaver_mode(void);

// When disabled the saver never starts on its own; a manual preview still
// works.
bool screensaver_enabled(void);
void screensaver_set_enabled(bool on);

// Idle delay before the saver starts, in seconds (clamped to 5..3600).
void screensaver_set_idle_seconds(uint32_t s);
uint32_t screensaver_idle_seconds(void);
uint64_t screensaver_idle_us(void);

// Start / stop the active demo.  `img` is a 128x64 1bpp (SSD1306 page format)
// image used by the puzzle demo; the other demos ignore it (NULL is fine).
void screensaver_begin(const uint8_t *img);
void screensaver_stop(void);
bool screensaver_active(void);

// Advance the animation, drawing into dst (SSD1306_BUF_LEN bytes).  Returns
// true when dst changed and should be rendered.
bool screensaver_step(uint8_t *dst);

#endif // SCREENSAVER_H
