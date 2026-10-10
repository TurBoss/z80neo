# z80neo bare-metal graphics demos

Small C demos for the z80neo, built with the in-tree z88dk `+z80neo` target
and its VoidVDP bitmap API from `<z80neo.h>`.  They link at `0x0000`, own page
0, and drive the console on ports `0x80` / `0x81`.

## Build

```sh
make                 # -> build/<name>.ihx for every demo
make Z88DK=/path/to/z88dk
```

## Deploy

1. Copy a `build/<name>.ihx` to the SD-card root.
2. Point the first program in `Z80NEO.INI` at it (`BANK_PROG[0]`):

   ```
   cpmz80neo.hex   ->   stars.ihx
   ```
3. Reset the board.  Output is UART0 (115200 8N1) to the VDP.

Press `q` or `ESC` to quit a demo and return the VDP to text mode.

## Demos

| File | What it draws |
|------|---------------|
| `stars.c`     | 90 drifting stars, each with its own palette colour |
| `bars.c`      | 80 animated colour bars |
| `cube.c`      | rotating 3D wireframe cube (12 hardware lines/frame) |
| `lissajous.c` | three parametric curves; a key redraws with a new phase |
| `wave.c`      | two animated sine waves drawn as line segments |
| `balls.c`     | five bouncing colour blocks |
| `logo.c`      | animated neon `Z80NEO` wordmark (vector strokes, glow, colour wave) |

`common.h` has a 64-entry fixed-point sine table, an LCG, and a non-blocking
key check, so the demos use no `<math.h>` and no `rand()`.

## Display note

On an **RP2350** VDP the bitmap layer works on both **DVI and VGA**; on an
RP2040 VDP it is available on DVI only (1bpp, two colours per cell), and on
RP2040 VGA the `z80neo_gfx_*` calls are no-ops.  The PC `sdl_client`
implements the same `ESC g` protocol, so it renders these demos too (no DVI or
VGA display needed).

The **NullVDP** (Tang Primer 20K FPGA VDP, `vdp/NullVDP/`) implements the same
`ESC g` protocol, so these demos run on it unchanged.  Its bitmap layer is
**8 colours** (3bpp); a colour byte's low 3 bits select palette entries 0-7, so
the bright half of the palette (8-15) appears as its base colour.

On RP2350 the layer is **true-colour**: every pixel stores its own display
colour, so drawing a shape after `z80neo_gfx_color()`, or using
`z80neo_gfx_pixel()`, gives per-pixel colour.

## API

```c
#include <z80neo.h>

void z80neo_gfx_bitmap(uint8_t on);                    /* 1 = bitmap, 0 = text */
void z80neo_gfx_clg(void);                             /* clear to bg colour */
void z80neo_gfx_color(uint8_t fg, uint8_t bg);         /* 0-15 */
void z80neo_gfx_plot(uint16_t x, uint16_t y, uint8_t on);
void z80neo_gfx_pixel(uint16_t x, uint16_t y, uint8_t colour);   /* per-pixel colour */
void z80neo_gfx_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint8_t on);
void z80neo_gfx_fill(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t on);
```

Coordinates are 0-639 x 0-255.  Lines and rectangles are rendered by the VDP,
so they cost one command each; pixels are one command per pixel.

Note: `z80neo_gfx_clg()` clears to the current **background** colour, so call
`z80neo_gfx_color(fg, 0)` first if you want a black background.

The driver lives in `software/z88dk/libsrc/target/z80neo/gfx/z80neo_gfx.c`
and the prototype in `software/z88dk/include/z80neo.h`.  After changing either,
rebuild the target library:

```sh
cd software/z88dk
export PATH=$PWD/bin:$PATH ZCCCFG=$PWD/lib/config
make -C libsrc z80neo_clib.lib
cp libsrc/z80neo_clib.lib lib/clibs/z80neo_clib.lib
```
