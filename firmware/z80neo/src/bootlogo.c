// bootlogo.c — graphical z80neo boot logo for the VDP / sdl_client
// z80neo — TurBoss 2026
//
// Sends the same VoidVDP "ESC g" bitmap protocol that the z80dk
// z80neo_gfx_* layer emits (see common/z80neo_protocol.h, doc/vdp_protocol.md),
// directly over the console UART while the Z80 is held in reset.  The wordmark
// is a 7-segment vector: each stroke is filled twice, a dim halo then the
// bright tube, so it reads as neon on black.  It is revealed glyph by glyph,
// held, then the bitmap layer is switched off so the text console continues.

#include "bootlogo.h"

#include <pico/stdlib.h>

#include "memory.h"

#define LOGO_W   64
#define LOGO_H   104
#define LOGO_T   14
#define LOGO_GAP 16
#define LOGO_N   6
#define LOGO_TOP 76

// Stroke set bits.
#define SA 0x01
#define SB 0x02
#define SC 0x04
#define SD 0x08
#define SE 0x10
#define SF 0x20
#define SG 0x40

static void vput(uint8_t c) {
    uart_putc(UART_ID, c);
}

static void vcmd(uint8_t c) {
    vput(27);
    vput('g');
    vput(c);
}

static void vword(uint16_t v) {
    vput((uint8_t)v);
    vput((uint8_t)(v >> 8));
}

static void vbitmap(uint8_t on) {
    vcmd(0x01);
    vput(on);
}

static void vcolor(uint8_t fg, uint8_t bg) {
    vcmd(0x03);
    vput(fg);
    vput(bg);
}

static void vfill(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t on) {
    vcmd(0x06);
    vword(x);
    vword(y);
    vword(w);
    vword(h);
    vput(on);
}

static void vline(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint8_t on) {
    vcmd(0x05);
    vword(x0);
    vword(y0);
    vword(x1);
    vword(y1);
    vput(on);
}

// One stroke rectangle plus its halo, in colour `c` (halo = c - 8).
static void stroke(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t c) {
    if (c > 8) {
        vcolor((uint8_t)(c - 8), 0);
        vfill((uint16_t)(x - 3), (uint16_t)(y - 3), (uint16_t)(w + 6),
              (uint16_t)(h + 6), 1);
    }
    vcolor(c, 0);
    vfill(x, y, w, h, 1);
}

static uint8_t glyph_segs(char k) {
    switch (k) {
    case 'Z':
        return SA | SB | SG | SE | SD;
    case '8':
        return SA | SB | SC | SD | SE | SF | SG;
    case '0':
    case 'O':
        return SA | SB | SC | SD | SE | SF;
    case 'N':
        return SB | SC | SE | SF;
    case 'E':
        return SA | SF | SG | SE | SD;
    }
    return 0;
}

static void draw_glyph(uint16_t x, char k, uint8_t c) {
    uint8_t s = glyph_segs(k);
    uint16_t hh = LOGO_H / 2;
    uint16_t vh = (uint16_t)(hh - LOGO_T);
    uint16_t rx = (uint16_t)(x + LOGO_W - LOGO_T);
    uint16_t my = (uint16_t)(LOGO_TOP + hh - LOGO_T / 2);

    if (s & SA)
        stroke((uint16_t)(x + LOGO_T), LOGO_TOP, (uint16_t)(LOGO_W - 2 * LOGO_T),
               LOGO_T, c);
    if (s & SG)
        stroke((uint16_t)(x + LOGO_T), my, (uint16_t)(LOGO_W - 2 * LOGO_T),
               LOGO_T, c);
    if (s & SD)
        stroke((uint16_t)(x + LOGO_T), (uint16_t)(LOGO_TOP + LOGO_H - LOGO_T),
               (uint16_t)(LOGO_W - 2 * LOGO_T), LOGO_T, c);
    if (s & SF)
        stroke(x, (uint16_t)(LOGO_TOP + LOGO_T), LOGO_T, vh, c);
    if (s & SE)
        stroke(x, (uint16_t)(LOGO_TOP + hh), LOGO_T, vh, c);
    if (s & SB)
        stroke(rx, (uint16_t)(LOGO_TOP + LOGO_T), LOGO_T, vh, c);
    if (s & SC)
        stroke(rx, (uint16_t)(LOGO_TOP + hh), LOGO_T, vh, c);
    if (k == 'N') {
        // A single hardware line is 1px; stack parallel lines shifted along X
        // (which the end bars hide) so the diagonal matches the stroke width
        // and gets the same halo as the rest of the glyph.
        int s;
        if (c > 8) {
            vcolor((uint8_t)(c - 8), 0);
            for (s = -(LOGO_T / 2 + 3); s <= LOGO_T / 2 + 3; s++)
                vline((uint16_t)(x + LOGO_T + s), (uint16_t)(LOGO_TOP + LOGO_T),
                      (uint16_t)(x + LOGO_W - LOGO_T + s),
                      (uint16_t)(LOGO_TOP + LOGO_H - LOGO_T), 1);
        }
        vcolor(c, 0);
        for (s = -(LOGO_T / 2); s <= LOGO_T / 2; s++)
            vline((uint16_t)(x + LOGO_T + s), (uint16_t)(LOGO_TOP + LOGO_T),
                  (uint16_t)(x + LOGO_W - LOGO_T + s),
                  (uint16_t)(LOGO_TOP + LOGO_H - LOGO_T), 1);
    }
}

void boot_console_reset(void) {
    static const char ansi[] = "\033[2J\033[H";   // ANSI clear + home
    vbitmap(0);                    // a prior gfx program may have left it on
    for (const char *p = ansi; *p; p++)
        vput((uint8_t)*p);
}

void boot_logo_show(void) {
    static const char kinds[LOGO_N] = { 'Z', '8', '0', 'N', 'E', 'O' };
    const uint16_t bw = (uint16_t)(LOGO_N * LOGO_W + (LOGO_N - 1) * LOGO_GAP);
    const uint16_t x0 = (uint16_t)((640 - bw) / 2);
    const uint16_t by = (uint16_t)(LOGO_TOP + LOGO_H + 14);

    vcolor(0, 0);
    vbitmap(1);                         // also clears to the bg colour
    for (uint8_t i = 0; i < LOGO_N; i++) {
        draw_glyph((uint16_t)(x0 + i * (LOGO_W + LOGO_GAP)), kinds[i], 15);
        sleep_ms(120);
    }

    // Underline rail, then hold the logo before handing back the text console.
    vcolor(8, 0);
    vfill(x0, by, bw, 4, 1);
    vcolor(15, 0);
    vfill((uint16_t)(x0 + bw / 3), by, (uint16_t)(bw / 4), 4, 1);
    sleep_ms(1200);

    vbitmap(0);
}
