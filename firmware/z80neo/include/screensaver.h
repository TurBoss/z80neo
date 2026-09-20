#ifndef SCREENSAVER_H
#define SCREENSAVER_H

#include <stdbool.h>
#include <stdint.h>

// 15-puzzle screensaver for the idle OLED screen.  The supplied 128x64 1bpp
// image (SSD1306 page format) is split into a 4x4 grid of 32x16 tiles and slid
// around as a 15-puzzle: the image is revealed, scrambled, then solved by
// replaying the scramble in reverse, and the cycle repeats.

void screensaver_begin(const uint8_t *img);
void screensaver_stop(void);
bool screensaver_active(void);

// Advance the animation, drawing into dst (SSD1306_BUF_LEN bytes).  Returns
// true when dst changed and should be rendered.
bool screensaver_step(uint8_t *dst);

#endif // SCREENSAVER_H
