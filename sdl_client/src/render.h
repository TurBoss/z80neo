#ifndef RENDER_H
#define RENDER_H

#include "terminal.h"

#include <stdbool.h>
#include <stdint.h>

#define RENDER_CELL_W 8
#define RENDER_CELL_H 16

void render_frame(const Terminal *t, uint32_t *pixels, bool blink_on,
				  bool flash);

#endif
