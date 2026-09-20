# VDP console protocol

The Z80 board talks to its display over the serial console.  The **VoidVDP**
firmware (`vdp/`) is the reference implementation; the PC **sdl_client**
(`sdl_client/`) implements the same protocol so the Z80 can be driven from a
desktop.  This document is the shared specification; the machine-readable
graphics command table lives in `common/z80neo_protocol.h` and is included by
both `vdp/src/terminal.c` and `sdl_client/src/terminal.c` so the command bytes
and payload lengths cannot drift.

Transport: raw bytes on the console.  On the VDP that is UART (`serial.c`, the
Z80's UART0 console), and on the Z80 side port `0x80` (data) / `0x81` (status).
On the PC client it is the serial port (`/dev/ttyACM0` by default).

## 1. Text

ASCII text, plus the VT102 subset below.  Control codes:

| Code | Meaning |
|------|---------|
| `BEL` 0x07 | bell (VDP: audible + visual, configurable; client: tone + flash) |
| `BS` 0x08, `DEL` 0x7f | cursor left (VDP: `recvBS`/`recvDEL` config) |
| `HT` 0x09 | tab to next stop (default every 8 columns) |
| `LF` 0x0a, `VT` 0x0b, `FF` 0x0c | cursor down one row (scroll at region bottom) |
| `CR` 0x0d | cursor to column 0 |
| `SO` 0x0e / `SI` 0x0f | select G1 / G0 charset |
| `ENQ` 0x05 | answerback message (VDP config) |

`ESC` sequences:

| Sequence | Meaning |
|----------|---------|
| `ESC c` | reset (RIS) |
| `ESC 7` / `ESC 8` | save / restore cursor (position, colours, charsets, origin) |
| `ESC H` | set tab stop |
| `ESC J` / `ESC K` | erase to end of screen / row |
| `ESC D` | index (cursor down) |
| `ESC E` | next line (column 0 + cursor down) |
| `ESC M` | reverse index |
| `ESC Z` | DECID -> `ESC [ ? 6 c` |
| `ESC ( ) * + <c>` | designate G0-G3 charset (`A`=UK, `B`=US, `0`=DEC graphics) |
| `ESC # 3 / 4 / 6` | double-width / double-height-top / double-width (VDP only) |
| `ESC # 5` | normal size (VDP only) |
| `ESC # 8` | fill screen with `E` (DEC test) |
| `ESC g ...` | graphics protocol (section 3) |

CSI (`ESC [` … final).  Final characters:

- Cursor: `A B C D E F G H I Z \` a d e f j k`
- Erase / edit: `J K X @ P L M S T`
- Modes: `h` / `l` (see below), `r` (scroll region), `s` / `u` (save/restore)
- SGR: `m`
- Tabs: `g`
- Reports: `c` (device attributes), `n` (status / cursor position)

Private modes (`CSI ? n h` / `l`):

| n | Meaning |
|---|---------|
| 2 | VT52 mode (off = reset + VT52) |
| 3 | 80/132 column switch (clears) |
| 4 | smooth scrolling (VDP scroll delay; no-op on the client) |
| 5 | reverse video |
| 6 | origin mode (DECOM) |
| 7 | auto-wrap (DECAWM) |
| 12 | local echo (VDP configurable; no-op on the client) |
| 25 | cursor visible |

Non-private `CSI 4 h/l` is insert mode (IRM).

DEC SCP / reports:

- `ESC [ c` (and `ESC [ ? 1 ; 0 c`) -> `ESC [ ? 6 c` (VT102)
- `ESC [ 5 n` -> `ESC [ 0 n`
- `ESC [ 6 n` -> `ESC [ <row> ; <col> R` (1-based, origin-relative)

VT52 mode supports `A B C D E H I J K L M Y Z b c d e f j k l o p q v w <`.

## 2. Colours

The terminal has 16 colours (CGA/ANSI order 0-15) with the palette configured
in the VDP menu.  SGR supports `0 1 4 5 7 22 24 25 27`, foreground `30-37`
and bright `90-97`, background `40-47` and bright `100-107`, `38;5;n` /
`48;5;n` (n & 15), and `39`/`49` for the defaults.

BBC BASIC's `COLOUR` maps to these: `COLOUR 0-7` normal fg, `8-15` bright fg,
`128-135` bg, `136-143` bright bg.

## 3. Graphics protocol (`ESC g`)

Frame: `ESC 'g' <cmd> <payload>`.  Coordinate words are little-endian
(`lo hi`).  The bitmap layer is 640x256, aligned with the text grid and centred
vertically.  Full table in `common/z80neo_protocol.h`:

| cmd | payload | action |
|-----|---------|--------|
| 0x01 | `on` | bitmap mode on (1) / off (0); turning on clears |
| 0x02 | `0` | clear the bitmap |
| 0x03 | `fg bg` | set the current foreground/background palette colours |
| 0x04 | `x(2) y(2) on` | set/clear a pixel using the current colours |
| 0x05 | `x0(2) y0(2) x1(2) y1(2) on` | line (hardware) |
| 0x06 | `x(2) y(2) w(2) h(2) on` | filled rectangle (hardware) |
| 0x07 | `x(2) y(2) colour` | set a pixel to an explicit palette colour |

Rendering differences by display:

| Target | Graphics layer |
|--------|----------------|
| VDP on **RP2350**, DVI | **true colour**: one display-colour byte per pixel (160 KB buffer), TMDS-paletted |
| VDP on **RP2350**, VGA | **true colour**: the same buffer as a native 8-bit R3G3B2 layer |
| VDP on **RP2040**, DVI | 1bpp, two colours (`fg`/`bg`) per 8x16 character cell |
| VDP on **RP2040**, VGA | no graphics layer (the RP2040 lacks RAM for it) |
| **sdl_client** | true colour, same 16-colour palette |

So on an RP2350 VDP (either output) drawing different shapes after `ESC g 0x03`
(or using command 0x07) gives per-pixel colour; on RP2040 DVI the colour is
quantised to two per character cell, and on RP2040 VGA there is no layer.  On
VGA the layer is shown at the top of the screen (256 lines); on DVI it is
centred vertically.

BBC BASIC (`software/bbcbasic-z80/hardware.asm`) drives this from `MODE`,
`GCOL`, `CLG`, `PLOT`, `MOVE` and `DRAW`.

### z88dk driver

`software/z88dk/libsrc/target/z80neo/gfx/z80neo_gfx.c` implements the same
protocol for bare-metal C, exposed in `software/z88dk/include/z80neo.h`:

```c
z80neo_gfx_bitmap(on);  z80neo_gfx_clg();
z80neo_gfx_color(fg, bg);
z80neo_gfx_plot(x, y, on);
z80neo_gfx_pixel(x, y, colour);      /* command 0x07 */
z80neo_gfx_line(x0, y0, x1, y1, on);
z80neo_gfx_fill(x, y, w, h, on);
```

## 4. Intentional differences

- **PETSCII** mode is VDP-only (configured in the menu); the client is
  VT102/VT52 only.
- **Double width/height** (`ESC # 3/4/6`) is VDP-only; the client accepts and
  ignores it.
- While bitmap mode is active the VDP renders text *into* the bitmap; the
  client renders only the bitmap layer (the text grid is kept undamaged and
  reappears when bitmap mode is turned off).
- The VDP has runtime-configurable receive CR/LF/BS/DEL handling, bit-7
  stripping, answerback, cursor shape, scroll delay and bell; the client uses
  the VDP defaults (Enter=CR, BS=0x08, DEL=0x7f).
