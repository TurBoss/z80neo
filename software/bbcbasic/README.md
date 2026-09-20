# BBC BASIC demos for the z80neo

A small collection of BBC BASIC programs for the z80neo, written for the
CP/M build of **BBC BASIC (Z80)** in [`../bbcbasic-z80`](../bbcbasic-z80).
They are plain ASCII listings, not tokenised `.BBC` files: load one with
`*EXEC`, which replays it as if typed (see below).

## Install

`install.sh` copies the listings onto a CP/M disk image with `cpmcp`:

```sh
./install.sh                 # -> ../../sdcard/CPMDISK0.IMG (drive A:)
./install.sh other.img       # any z80neo-geometry image
```

BBCBASIC.COM is already present on `CPMDISK0.IMG`.

## Run

Boot CP/M, then:

```
A>BBCBASIC
> *EXEC STARS
```

`*EXEC` feeds the listing to the keyboard, so the numbered lines are entered
and the trailing `RUN` starts the demo. Press `ESC` to break out (you may need
`*ESC ON` if you turned abort off). Once a demo is in memory it can be turned
into a normal tokenised program with `SAVE "STARS"`; after that `LOAD "STARS"`
(or `BBCBASIC STARS`) works, and `*EXEC STARS` will no longer find the ASCII
listing.

## The demos

| File | What it does | Display |
|------|--------------|---------|
| `stars.bas`  | Starfield: 25 stars of three brightnesses drift down-left across a cleared screen | ANSI text |
| `rain.bas`   | "Matrix" rain: 80 columns, a dozen updated per frame, green glyphs with a short trail | ANSI text |
| `wave.bas`   | Animated sine wave drawn with solid blocks | ANSI text |
| `bounce.bas` | Three solid blocks bouncing around the screen | ANSI text |
| `sierp.bas`  | Sierpinski triangle from Pascal's triangle (mod 2), one-shot | ANSI text |
| `cube.bas`   | Rotating 3D wireframe cube | VDP bitmap (DVI) |
| `liss.bas`   | Two Lissajous curves | VDP bitmap (DVI) |

The ANSI text demos use only `CLS`, `COLOUR 0..7` and cursor addressing, so
they display on the **VoidVDP** terminal *and* on the PC **sdl_client**. The
two bitmap demos need the VDP's DVI output (the firmware's bitmap layer is
DVI-only); on a VGA build, or under `sdl_client` (which ignores the `ESC g`
graphics protocol), they will draw nothing.

## Notes

Cursor positioning is done with raw ANSI `ESC[row;colH` built in the program
rather than BBC's `TAB(x,y)`: the CP/M hardware layer emits `ESC[x;yH` from
`TAB` (column first), which is transposed relative to a standard ANSI
terminal. The helper at the top of each listing avoids that.

Colours are restricted to `COLOUR 0..7`. The port maps bright values
(`COLOUR 8..15`) to ANSI `90..97`/`100..107`, which the VDP does not currently
parse, so they would be ignored there.

The Z80 is clocked at ~128 kHz (see the firmware `CPU_SPEED`), so these are
tuned to do little work per frame; `wave.bas` and `sierp.bas` are the heaviest.
If you raise the clock, the animation gets correspondingly smoother.
