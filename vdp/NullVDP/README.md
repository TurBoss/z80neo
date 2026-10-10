# NullVDP

An FPGA VDP for the z80neo, written in Verilog and built with the open-source
Gowin toolchain (oss-cad-suite).  Target board: **Sipeed Tang Primer 20K**
(GW2A-LV18PG256C8/I7) + Dock, HDMI output.

The goal is a hardware implementation of the z80neo console/VDP protocol
(`doc/vdp_protocol.md`, `common/z80neo_protocol.h`) that the z80neo drives over
**SPI** instead of the UART link used by the RP2350 `VoidVDP` firmware.

## Provenance

Forked from the `dummy_terminal` example in
[mikewolak/tangprimer20k-oss-examples](https://github.com/mikewolak/tangprimer20k-oss-examples)
— a working 80×30 HDMI text terminal for this board.  The video pipeline
(`hdmi_pll`, `tmds_*`, `text_controller`, `font_rom`, `char_buffer`,
`sync_cursor`) and the UART are carried over from that example; NullVDP
renames the project and extends the terminal toward the z80neo protocol.

## Status

**Phase 1 (bring-up fork): done.** Builds and behaves as the upstream example.

**Phase 2 (colour text): done.** The terminal now implements the z80neo text
protocol with 16-colour attributes:
- 80×30 grid, 8×16 font, per-cell foreground/background palette index plus
  underline/blink/inverse/attribute bits (`attr_buffer.v`).
- 16-colour CGA/ANSI palette (`palette.v`), matching `sdl_client`/VoidVDP.
- Cursor motion (A B C D E F G H f d), erase (J K 0/1/2), save/restore
  (ESC 7/8 and CSI s/u), ESC c reset, index/reverse (ESC D/E/M).
- SGR `m`: 0 1 4 5 7 22 24 25 27, 30-37/40-47, 90-97/100-107, 39/49 and
  38;5;n / 48;5;n (processed one parameter per cycle).
- Private modes ?25 (cursor) and ?7 (autowrap); ESC ( ) * + # designators and
  the `ESC g` graphics payload are consumed so they never print as text.
- Full-screen hardware scroll via a 30-row offset (a scroll is one register
  update plus clearing the exposed line).
- A power-on reset blanks the screen at configuration (the button is already
  released then, so a plain `rst_n` never fires).

**Hardware bring-up: done.** Validated on a Tang Primer 20K + Dock over HDMI:
text, SGR colour, cursor and UART input all work (the UART echoes received
bytes back).

**SPI transport: done (phase 3).**  `src/spi_slave.v` implements the SPI mode-0
slave and `sim/tb_spi.v` covers it (`make sim-spi`).  Console bytes from the
z80neo fill the terminal (priority over UART/PS-2) and PS/2 bytes are pushed to
the host over the RX back-channel.  See `doc/nullvdp_spi_protocol.md` for the
register map and pinout.  The Pico-side master lives in the z80neo firmware
(`src/spi_vdp.c` + `src/spi_vdp.pio`).

**Bitmap graphics (`ESC g`): done (phase 4).**  The `ESC g` commands
(`common/z80neo_protocol.h`, `doc/vdp_protocol.md`) now draw into a 640×256
bitmap layer stored as three 1-bit BSRAM planes (`src/gfx_buffer.v`,
163840 = 10 blocks each).  `src/gfx_engine.v` plots pixels, Bresenham lines and
filled rectangles (and clear/mode-on as a full-screen fill) into the planes;
`src/terminal_handler.v` collects each command's payload and dispatches it,
holding the console (`rx_ready` low) until the engine finishes.  The renderer
(`src/text_controller.v`) shows the layer centred vertically (112-line bars)
instead of the text grid while bitmap mode is active; the F12 menu still
overlays on top.  The layer is **8 colours** (3bpp): the low 3 bits of a colour
byte select palette entries 0–7.  This means the z88dk
`+z80neo` graphics API (`z80neo_gfx_*`) and the `software/z80neo/demos/` run
unchanged on the NullVDP.  `make sim-gfx` covers the command path.

The text grid is left intact while bitmap mode is on and reappears when it is
turned off.  Unlike the RP2350 VoidVDP, text is **not** yet rendered *into* the
bitmap (printing while in graphics mode keeps writing the hidden text grid).

**Options menu (F12): done.**  Open/close with F12; navigate with Up/Down;
change a value with Left/Right; Enter runs the selected action or opens an info
page; Esc backs out / closes.  Settings are applied live: **Cursor** visibility,
**Local echo** (AUTO/ON/OFF: AUTO echoes only when no host is driving the link,
ON forces local echo even with the z80neo attached, OFF never echoes locally),
**Palette** (normal/bright).  Because the z80neo firmware polls the SPI link
continuously, AUTO always defers to the host's echo — select ON to type locally.
Actions: **Clear screen** and **Reset terminal** inject the matching terminal
sequence.  Info pages: Help, About, Serial/terminal, Colours, Credits.  While
the menu is open the keyboard is consumed locally — nothing is forwarded to the
z80neo over SPI or UART.

**USB keyboard: standalone only — cannot coexist with HDMI.** The Dock's
"USB OTG" port (USB3317 ULPI PHY) is driven as a USB low/full-speed **host**
(`ulpi_host.v`); key presses are mapped to console bytes and to the menu's CSI
sequences (`hid_expand.v`).  This works in the standalone `usb_kbd_top`
bitstream (no video), but **not together with HDMI** — see "HDMI + USB bank
conflict" below.  The shipping `nullvdp_top` therefore uses the Dock's **PS/2**
keyboard (PMOD), which coexists with HDMI: PS/2 keys type into the local
console and drive the F12 menu, and are also forwarded to the host on
`uart_tx`.

**HDMI + USB bank conflict.** HDMI output requires **true-LVDS**
(`TLVDS_OBUF`), which puts IO banks 0/1 into LVDS mode (`LVDS_OUT=ON`).  The
Dock's ULPI pins for `ulpi_data[0..3]`, `ulpi_dir/nxt/stp` live in those same
banks (IOT14/18/36/38), and an LVDS-mode bank cannot host the ULPI single-ended
LVCMOS buffers.  Verified by bisection on hardware: HDMI-only works; HDMI plus
only the ULPI I/O buffers (no USB logic, no second clock) shows no signal;
reworking TMDS to single-ended LVCMOS (fabric serializer, homogeneous bank)
also does not drive the monitor here.  So HDMI and the Dock USB host are
mutually exclusive with this board + open flow.  Use PS/2 for a keyboard with
video, or the standalone USB bitstream for a USB keyboard without video.

Verified:
- `yosys synth_gowin` → `nextpnr-himbaechel` → `gowin_pack` → `nullvdp.fs`,
  flashed over the Sipeed JTAG debugger with `openFPGALoader -b tangprimer20k`.
- Resource use (with the bitmap layer): **3940 LUT4 (19%), 1312 DFF (8%),
  39/46 BSRAM (84%), 1 rPLL**.
- `make sim` (iverilog) exercises the boot clear, text + attributes, SGR,
  cursor/erase and scroll mapping — all pass.
- `make sim-gfx` exercises the `ESC g` path (mode/clear/colour/pixel/line/fill)
  end to end through `terminal_handler` → `gfx_engine` → `gfx_buffer`.
- `sim/tb_video.v` checks the handler → buffers → renderer path end to end.

Known limits: the bitmap layer is 8 colours (see phase 4 above) and text is not
rendered into it; the font ROM is 128 glyphs (CP437 high bytes render as their
low 7 bits); scroll region (`CSI r`) and insert/delete are accepted but not
implemented.  The input FIFO is currently bypassed (see below).

## HDMI bring-up notes (GW2A + Dock)

Getting HDMI out of this board with the open-source flow took some finding.
The working reference is the **apicula DVI example** (`apicula/examples/DVI/`).

- **Clock**: `pll480.v` (verbatim from the example) gives 126 MHz = 5x pixel;
  `CLKDIV("5")` → 25.2 MHz.  (apycula computes `CLKOUT = FCLKIN*(FBDIV+1)/(IDIV+1)`.)
- **`serial_clk` must clock NOTHING but the OSER10 `FCLK`.**  Using it as a
  fabric clock (e.g. a heartbeat counter) breaks the IOLOGIC high-speed-clock
  routing and the monitor shows "no signal".
- **Constraints**: the TMDS pins are `PULL_MODE=NONE DRIVE=3.5` with **no
  `IO_TYPE`** — the IO standard comes from `TLVDS_OBUF`.  Setting
  `IO_TYPE=TMDS33` drives apycula into a half-implemented path and fails.
- **Sync**: HSYNC/VSYNC ride on TMDS channel 0 = **blue**, negative polarity
  for 640×480@60.  The original `dummy_terminal` had both wrong.
- **Encoder**: Sameer Puri's `tmds_channel.sv` (SystemVerilog) is used; the
  custom `tmds_encoder.v` is kept but not built.
- **Addressing**: the char/attr buffers are 2400 cells → **12-bit** addressing;
  11 bits aliased rows 26–29 onto the top ("text shown twice").
- **FIFO**: a register-based input FIFO perturbs placement enough to break the
  marginal 126 MHz clock routing, so the UART feeds the handler directly.  A
  BRAM-based FIFO (or a stable, constrained clock) is future work.

### apycula patches

The working bitstream uses two small local edits to
`~/Apps/oss-cad-suite/.../apycula/gowin_pack.py` (backup alongside as
`gowin_pack.py.orig`):

1. add `'TMDS33': '3.3'` to the `_vcc_ios` table (apycula knew the IO type but
   not its VCCIO), and
2. in `check_io_banks`, force the LVDS bank VCCIO from the output's `IO_TYPE`
   rather than the fixed 1.2 V.

## USB keyboard notes (USB3317 on the Dock)

The Dock's user USB port is a **USB3317 ULPI PHY**; the Sipeed
`TangPrimer-20K-example` USB example is the pin/clock reference.  Its ULPI pins
span IO banks 0/1/2/7 (the bank 0/1 pins are the ones that conflict with the
HDMI TMDS outputs — see "HDMI + USB bank conflict").  The PHY's
ULPI clock is inverted in the fabric (`~ulpi_clk`), and `CPEN` is not connected
while the connector VBUS is hard-wired to the board +5 V, so the keyboard is
powered independent of host-mode VBUS drive.

Three things were wrong and are fixed here:

1. **No USB bus reset.** `Function Control` was written with `OpMode=00`
   (`0x40`, normal/non-driving).  A USB reset needs `OpMode=10` (`0x50`) to
   drive SE0; without it a device never resets and ignores all traffic.
2. **Truncated control-IN transfers.** The enumeration requested 18/9 bytes but
   read only the first 8-byte packet before the status stage.  A low-speed
   keyboard's ep0 max packet is 8, so the device STALLs the premature status.
   The requests were reduced to one packet (the descriptors are not parsed).
3. **`hid_expand` repeated the last key** because `nlen` was not cleared after
   a sequence completed.

Hardware setup: the slide switch (`SW2`, "change USB Interface function")
selects the PHY `ID` pin — put it in the position that pulls **ID to GND** for
host/A-device mode.  Note the OTG receptacle's CC1/CC2 are 5.1 kΩ to GND (a
device/sink strap), so an old USB-A keyboard through an A-to-C cable is the
most reliable choice.

The strict low-speed-keyboard simulation lives outside the tree (the model
enforces a bus reset and an 8-byte ep0); `make sim-usbkbd` covers the HID
decode + clock-domain crosser.

## Build

```sh
./build.sh                 # -> nullvdp.fs
./build.sh program         # also flash over the on-board USB-JTAG
./build.sh sim             # iverilog simulation of the terminal handler
# or, with oss-cad-suite already sourced:
make ; make program ; make sim
# other sims: make sim-menu sim-ps2 sim-usbkbd sim-ulpi sim-ulpi-host sim-spi sim-gfx
```

**Keep `SEED=3`.**  Placement seed 3 is required for this board's marginal
126 MHz OSER10 serial-clock routing; other seeds can show snow or no signal.
`openFPGALoader -b tangprimer20k -f nullvdp.fs` writes the config flash
(persistent); without `-f` it is a volatile SRAM load.

`build.sh` sources oss-cad-suite from `$OSS_CAD_SUITE`, `~/Apps/oss-cad-suite`,
`~/oss-cad-suite` or `/opt/oss-cad-suite` if `yosys` is not already in `PATH`.

## Pins (Tang Primer 20K + Dock)

| Signal | Pin(s) | Notes |
|--------|--------|-------|
| `clk_27m` | H11 | 27 MHz system clock |
| `rst_n` | T10 | button |
| `uart_rx` / `uart_tx` | T13 / M11 | bring-up input, 115200 8N1 |
| `led[3:0]` | L16 L14 N14 N16 | PLL lock / activity (UART·PS2·SPI) / menu open / HDMI reset released |
| `ps2_clk` / `ps2_data` | T7 / P6 | PS/2 keyboard (bank 3, IOR29A/IOR53A, 3.3 V; change to match your socket) |
| `spi_sck` / `spi_mosi` | N6 / A11 | NullVDP SPI slave, from the z80neo Pico (Dock PMOD03, banks 3/7, 3.3 V) |
| `spi_miso` / `spi_cs_n` | B11 / D11 | SPI return / active-low chip select (Dock PMOD03, 3.3 V) |
| `ulpi_clk` | T15 | USB3317 ULPI clock (~60 MHz, inverted in fabric) |
| `ulpi_dir` / `ulpi_nxt` / `ulpi_stp` | K12 / K13 / K11 | ULPI control |
| `ulpi_data[7:0]` | R12 P13 R13 T14 H13 J12 H12 G11 | ULPI bidirectional data |
| `ulpi_rst` | F10 | USB3317 RESETB |
| `tmds_clk_p/n` | G16 / H15 | HDMI clock |
| `tmds_data_p[0..2]` / `_n[0..2]` | H14 J15 K14 / H16 K16 K15 | HDMI R/G/B |

## Roadmap (phases)

1. **Fork + bring-up** — done.
2. **Colour text** — done (this stage).
3. **SPI transport** — done. `spi_slave.v` is a mode-0/32-bit framed slave
   with CRC-8 and sequence acknowledgements; the z80neo firmware has a PIO SPI
   master (`spi_vdp.c`) with Z80 ports `0x94` (data) / `0x95` (status) and a
   console mirror so CP/M/Zeal/demos work unchanged. Spec:
   `doc/nullvdp_spi_protocol.md`.
4. **Bitmap graphics (`ESC g`)** — done.  640×256 layer as three 1-bit BSRAM
   planes (10 blocks each, 30 total) → **8 colours** (3bpp).  4 bpp (16
   colours) would need ~36 blocks and does not fit the 46-block BSRAM
   alongside the text/overlay buffers; external SDRAM would be the escape hatch
   for a full 8-bit framebuffer.  Line/fill/clear run in hardware
   (`gfx_engine.v`); no text-into-bitmap yet.
5. **Keyboard** — PS/2 (PMOD) types into the local console and drives the F12
   menu, coexisting with HDMI.  A USB **host** on the Dock OTG port/USB3317 is
   implemented (`usb_kbd_top`) but, being in the same banks as the HDMI TMDS
   pins, cannot run in the same bitstream as video.  Still to do: return key
   bytes over the SPI back-channel for the firmware to inject into its RX ring.

## Files

| Path | Purpose |
|------|---------|
| `src/nullvdp_top.v` | top: PLL, SPI, terminal, video, TMDS |
| `src/terminal_handler.v` | byte stream → char/attr buffers, cursor, scroll |
| `src/text_controller.v` | 640×480 timing + colour rendering pipeline (12-bit addr) |
| `src/font_rom.v` | 8×16 font (128 glyphs) |
| `src/char_buffer.v` | dual-port BRAM text buffer (80×30, 12-bit addr) |
| `src/attr_buffer.v` | dual-port BRAM attribute buffer (fg/bg/flags) |
| `src/gfx_buffer.v` | dual-port BRAM bitmap layer (3× 1-bit planes, 640×256) |
| `src/gfx_engine.v` | `ESC g` drawing sequencer (pixel/line/fill/clear) |
| `src/palette.v` | 16-colour CGA/ANSI palette |
| `src/hdmi_pll.v`, `src/pll480.v` | clocking: 27 MHz → 126/25.2 MHz (PLL480 + CLKDIV/5) |
| `src/tmds_channel.v` | Sameer Puri's TMDS encoder (SystemVerilog) |
| `src/tmds_serializer.v`, `src/tmds_clock_serializer.v` | OSER10 + TLVDS_OBUF output |
| `src/uart_rx.v`, `src/uart_tx.v`, `src/sync_cursor.v` | UART + cursor/scroll CDC |
| `src/spi_slave.v` | z80neo SPI link slave (console in, keyboard back-channel) |
| `sim/tb_spi.v` | iverilog framed SPI master/slave test (`make sim-spi`) |
| `sim/tb_spi_gfx.v` | framed SPI → terminal → graphics replay test (`make sim-spi-gfx`) |
| `src/nullvdp.cst` | pin constraints |
| `src/input_fifo.v`, `src/tmds_encoder.v` | kept but currently unused |
| `sim/tb_terminal.v` | iverilog testbench for the protocol parser |
| `sim/tb_video.v` | iverilog handler→buffers→render test |
| `sim/tb_gfx.v` | iverilog `ESC g` command-path test (`make sim-gfx`) |
| `src/ulpi_link.v` | ULPI register window + USB packet TX/RX (single pad owner) |
| `src/ulpi_host.v` | USB host FSM: PHY init, bus reset, enumeration, HID polling |
| `src/usb_key_cdc.v` | USB key event CDC (ULPI clock → 27 MHz) |
| `src/hid_expand.v` | HID usage → console byte / menu CSI sequence |
| `sim/tb_usbkbd.v` | iverilog HID decode + CDC test |
| `sim/tb_ulpi_host.v`, `sim/ulpi_phy_model.v` | ULPI link/host protocol tests |
| `Makefile`, `build.sh` | oss-cad-suite build / program / sim |
