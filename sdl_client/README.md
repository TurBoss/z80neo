# z80neo SDL3 terminal client

A small desktop terminal for the z80neo, so the Z80 can be driven from a PC
instead of the on-board **VoidVDP** display. It opens a serial port
(`/dev/ttyACM0` by default — the debug probe bridging the z80neo UART0 console)
and emulates a **VT100/VT102** terminal in an SDL3 window: the same escape
sequences the VDP understands are parsed here and rendered with an 8x16 console
font.

It is a plain PC program, not firmware, and is not built by the Pico
toolchain.

## Build

Requires SDL3 (development package) and CMake:

```sh
cd sdl_client
./build.sh            # -> build/z80neo_client
```

`CMakeLists.txt` locates SDL3 through `pkg-config`. On a Debian/Ubuntu host that
is `libsdl3-dev`; the distro copy of `sdl3.pc` is enough.

## Usage

```sh
./build/z80neo_client [options]
  -d, --device <path>  serial device (default /dev/ttyACM0)
  -b, --baud <rate>    baud rate (default 115200)
  -s, --scale <n>      initial window scale (default 2 -> 1280x768)
  -c, --cols <n>       terminal columns (default 80)
  -r, --rows <n>       terminal rows (default 24)
      --vt52           start in VT52 mode
      --no-bell        mute the audible bell (visual flash remains)
  -h, --help
```

The console on the z80neo is **UART0, 115200 8N1** (GPIO0/1). A Raspberry Pi
Debug Probe with its UART bridged there shows up as a CDC-ACM device; the client
reconnects automatically every second if the port is not present yet.

## Keyboard

| Key | Sends |
|-----|-------|
| printable keys | the character (Latin-1 mapped to CP437 where possible) |
| Enter | CR (`0x0d`) |
| Backspace / Delete | `0x08` / `0x7f` (VDP defaults) |
| Tab | `0x09` |
| Ctrl-A..Z | `0x01`..`0x1a` |
| Ctrl-2/3/4/5/6/7/8, Ctrl-[ \\ ] - | `0`/ESC/`28`/`29`/`30`/`31`/`127`, `27`/`28`/`29`/`31` |
| Arrows | `ESC [ A/B/C/D` (VT52: `ESC A/B/C/D`) |
| F1-F4 | `ESC O P..S` (VT52: `ESC P..S`) |
| F5-F12 | `ESC [ 15~ .. 24~` |
| Home / End | `ESC H` / `ESC [ 4~` |
| Insert / Delete | `ESC [ 2~` / `ESC [ 3~` |
| PgUp / PgDn | `ESC [ 5~` / `ESC [ 6~` |
| Ctrl-Shift-V, Shift-Insert, middle-click | paste clipboard / primary selection |

## VT100/VT102 support

Text, `BEL`, `BS`, `TAB`, `LF`/`VT`/`FF`/`CR`, `SO`/`SI` charset shift and the
DEC special-graphics mapping (line drawing, blocks, `≤ ≥ π £`…) are handled, as
are:

- `ESC 7` / `ESC 8`, `ESC c`, `ESC D/E/H/M`, `ESC ( ) 0 A B`, `ESC # 8`, `ESC Z`
- CSI cursor motion `A B C D E F G H f d e a j k \``, tab `I Z g`
- erase `J K X`, insert/delete lines and chars `L M @ P`
- scroll region `r`, scroll `S T`, save/restore `s u`
- SGR `m` (bold, underline, blink, inverse, 30-37/40-47, 90-97/100-107,
  `38;5;n`/`48;5;n`)
- modes `?3 ?5 ?6 ?7 ?25` (private), `4` (IRM), and VT52 mode via `?2`
- device reports: `ESC [ c` -> `ESC [?6c`, `ESC [ 5n` -> `ESC [ 0n`,
  `ESC [ 6n` -> `ESC [ <row>;<col> R`

Incoming bytes `0x80`-`0xFF` are decoded as **CP437** so CP/M programs that draw
boxes with high bytes render correctly on the Unicode console font.

## Bell

A received `BEL` (`0x07`) rings the terminal like the VDP: a short 880 Hz tone
via SDL audio plus a brief full-screen flash. `--no-bell` mutes the tone; the
visual flash always fires, and audio is simply skipped if no device is
available.

The VDP-specific bitmap graphics protocol (`ESC g`) **is** rendered: a 640x256
true-colour layer, one 4-bit palette index per pixel, using the same 16-colour
palette as the text screen (`ESC g` commands 1-7: bitmap on/off, clear,
colour, pixel, line, filled rectangle, pixel-with-colour).  It is drawn
instead of the text grid while bitmap mode is active and centred vertically,
matching the VDP, so the bare-metal `z80neo_gfx_*` demos and BBC BASIC
graphics display here too.  (Text output is still parsed underneath; turning
bitmap mode off restores the text screen.)

## Font

`src/font8x16.h` is generated and committed. It bakes the Linux console
`default8x16` PSF2 font (256 CP437 glyphs plus a Unicode table) into a C header:

```sh
python3 tools/gen_font.py /usr/share/kbd/consolefonts/default8x16.psfu.gz src/font8x16.h
```

Any 8x16 PSF2 font with a Unicode table works. Rendering uses the tiny bitmap
font scaled by an integer factor.

## Test

`tools/` has no harness committed; the terminal emulator is easy to exercise by
linking `src/terminal.c` into a small program that feeds escape sequences and
inspects the cell grid.
