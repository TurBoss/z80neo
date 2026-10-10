# AGENTS.md — z80neo

Guidance for AI agents working in this repository.

## What this is

An Olimex **RP2350-PICO2-BB48** (RP2350**B**, 48-GPIO QFN-80 package) acting as
the RAM/ROM and all I/O for a **real Z80 CPU** (a Zilog **Z0840004**, NMOS,
TTL-level inputs). There is no software Z80 emulator in the firmware — the Pico
captures the Z80 bus cycle-by-cycle through PIO and services it, stalling the
Z80 with `WAIT`. It also emulates an MMU, a UART SIO, a 4-drive CP/M sector
disk, and drives an SSD1306 status display.

Do not confuse this with `tools/z80_boot_sim.py`, which is an offline Python
Z80 emulator used to debug the CP/M boot image.

## Repository layout

| Path | Purpose |
|------|---------|
| `firmware/` | Pico 2 firmware (C/C++/PIO), the main codebase |
| `firmware/boards/` | Custom pico-sdk board header (`z80neo_bb48.h`, RP2350B) |
| `firmware/z80neo/src/` | Firmware sources |
| `firmware/z80neo/include/` | Firmware headers |
| `firmware/z80neo/libs/pico_fatfs/` | Git submodule (SD/FatFS driver) |
| `software/` | Z80 side: CP/M 2.2 (`software/cpm/z80neo`), Fuzix (`software/Fuzix`, FCC-built banked kernel), OS (Zeal), `cpm/Collection/CPM_DISKS` (software library), assembler tests (gitignored) |
| `sdcard/` | SD-card payload: `Z80NEO.INI`, `cpmz80neo.hex`, `CPMDISK0..14.IMG`, test `.hex` files |
| `tools/` | Python helpers (hex combine, image converters, Z80 boot simulator, `pack_cpm_collection.py`, `install_cpm_disks.py`) + `TBCPMTools/` (PySide6 CP/M disk GUI) |
| `hardware/` | KiCad schematic/PCB |
| `doc/` | RP2350 BB48 datasheets and pinout, `vdp_protocol.md` (console/VDP protocol spec) |
| `common/` | Shared headers (`z80neo_protocol.h`: `ESC g` graphics commands, included by VDP + sdl_client) |
| `vdp/` | VoidVDP (VersaTerm fork): standalone RP2350/Pico 2 **VGA** serial terminal (a separate board, not the z80neo; see below) |
| `sdl_client/` | PC-side SDL3 VT100/VT102 serial terminal (CMake, not part of the Pico build); connects to `/dev/ttyACM0` |

## Build

Firmware build (`build.sh` hardcodes the local SDK paths — edit if yours differ):

```sh
cd firmware
./build.sh            # cmake -B build -S . -DPICO_BOARD=z80neo_bb48 -DPSRAM_ENABLE=ON; cmake --build build --parallel 1
```

The board is the Olimex RP2350-PICO2-BB48, which uses the RP2350**B** (48-GPIO)
package. `firmware/boards/z80neo_bb48.h` is a custom pico-sdk board header that
declares the B package (`PICO_RP2350A 0` → `NUM_BANK0_GPIOS=48`) and the 16 MB
W25Q128 flash; `firmware/CMakeLists.txt` selects it with
`PICO_BOARD=z80neo_bb48` + `PICO_BOARD_HEADER_DIRS=${CMAKE_CURRENT_LIST_DIR}/boards`.
**Do not build with `PICO_BOARD=pico2`**: that is the RP2350A (30 GPIOs), so
every `gpio_*`/PIO call for GPIO30-47 is silently rejected and those pins never
drive. `main.c` has an `#error` guard that fails the build in that case.

The output ELF is `firmware/build/z80neo/z80neo.elf`. `--parallel 1` is
deliberate.

Flash over CMSIS-DAP/OpenOCD:

```sh
cd firmware
./upload.sh           # openocd ... program build/z80neo/z80neo.elf verify reset exit
```

Debug (starts OpenOCD + arm-none-eabi-gdb):

```sh
cd firmware
./debug.sh            # standard gdbinit
./debug.sh serial     # serial-tracing gdb script (gdb_serial_debug)
```

There is **no unit-test framework**. Verify changes by building, flashing, and
watching UART0 (115200 8N1) plus the OLED. `tools/z80_boot_sim.py` is the only
offline test harness.

### Full pipeline (firmware + OS + SD deploy)

```sh
./build_all.sh        # builds + flashes firmware, builds Zeal OS, copies hex to mounted SD
```

`build_all.sh` assumes the SD card auto-mounts at `/run/media/turboss/Z80NEO`.
The OS itself builds from `software/Zeal/Zeal-8-bit-OS/` via `build.sh` there,
which sources `export.sh` (z88dk at `/home/turboss/Dev/z88dk`).

### Z80 assembler programs

Test programs live in `software/asm/` and are built with z88dk:

```sh
cd software/asm
./build_hex.sh serial_echo   # z88dk-z80asm -b serial_echo.s, then bin2hex.py
```

Copy resulting `.hex` to the SD card and reference it in `sdcard/Z80NEO.INI`.

### C programs (`zcc +z80neo`)

A bare-metal z88dk target lives in the in-tree toolchain (`software/z88dk`,
built): `+z80neo` links at `ORG 0x0000`, owns page 0, and talks to the console
on ports `0x80`/`0x81`. Build and load it exactly like an assembler program:

```sh
export PATH=$PWD/software/z88dk/bin:$PATH
export ZCCCFG=$PWD/software/z88dk/lib/config
zcc +z80neo prog.c -o prog -create-app     # -> prog.ihx (Intel HEX)
```

Copy `prog.ihx` to the SD root and set `BANK_PROG[0]` in `sdcard/Z80NEO.INI`.
`-subtype=bin` produces a raw binary instead. `getchar()` already echoes via
the classic stdio (`fgetc` calls `fputc_cons`), so don't echo again.
Target sources: `lib/config/z80neo.cfg`, `lib/target/z80neo/classic/`,
`libsrc/target/z80neo/`.

The target also ships the VoidVDP bitmap layer in `<z80neo.h>`: `z80neo_gfx_*`
(`bitmap`, `clg`, `color`, `plot`, `pixel`, `line`, `fill`) send the VDP
`ESC g` protocol on the console (640x256, 16 colours). On RP2350 the layer is
**true-colour** on both outputs — one display-colour byte per pixel in a 160 KB
buffer, paletted to TMDS on DVI and fed as a native R3G3B2 PicoVGA segment on
VGA — so `z80neo_gfx_color()` + `line`/`fill` and `z80neo_gfx_pixel()` give
per-pixel colour (`ESC g` command 7). The RP2040 VDP build lacks the RAM: it
keeps the 1bpp, two-colours-per-cell layer on DVI and has no graphics layer on
VGA. `framebuf_gfx_supported()` reports whether the active output has it. The
driver is
`libsrc/target/z80neo/gfx/z80neo_gfx.c`, wired into `z80neo_clib.lib` by
`z80neo.mak`; changing either needs a library rebuild
(`make -C software/z88dk/libsrc z80neo_clib.lib`, then copy it to
`lib/clibs/`). `software/z80neo/demos/` has graphics demos (`stars`, `cube`,
`lissajous`, `wave`, `balls`, `bars`) with a Makefile. Note sccz80 miscompiles
`a &= 0xff` on a `uint8_t` parameter (writes garbage); avoid that idiom.

The firmware's second UART (UART1 on GPIO38/39, Z80 ports `0x90` data / `0x91`
status) can be driven from bare-metal C too: `software/z80neo/uart1term/` is a
console<->UART1 bridge built with the same target (raw port I/O on 0x80/0x81 and
0x90/0x91). One CP/M equivalent exists as well:
`software/cpm/turboterm/` (executable `TTERM.COM`) bridges the CP/M console to
UART1 via BDOS direct console I/O + ports `0x90`/`0x91`.

UART1 line settings are runtime-configurable. The firmware exposes config ports
**`0x92`** (baud code) and **`0x93`** (format byte) — write to retune UART1, read
to get the current value. `TTERM.COM` sets them from its command line
(`TTERM [-b BAUD] [-f FMT]`, e.g. `TTERM -b 9600 -f 7E1`); the native
`uart1term` has no CLI and inherits the `Z80NEO.INI` defaults. Baud code indexes
`115200 57600 38400 19200 9600 4800 2400 1200 600 300 31250` (0..10; code 10 is
the MIDI rate, used by the `midiplayer`); the format byte
packs data bits as `db-5` in bits[1:0], 2 stop bits as bit3 and parity (0/1/2 =
none/even/odd) in bits[5:4]. Bit7 is set on a **read** to mark a firmware that
implements the port (`0x00` instead means an older firmware without it).

`software/z80neo/monitor/` is a full bare-metal machine monitor / debugger
built with the same target (dump/edit/fill/disassemble, breakpoints,
single-step, Intel HEX load, port I/O, MMU page control). It occupies page 0
and targets programs assembled for `0x4000`+ so it can drive code in MMU pages
1–3. See `software/z80neo/monitor/README.md`. Its `src/mon_asm.asm` overrides
the library's RST 38h handler with a trap that snapshots the whole CPU and
returns to the monitor; page 0 must stay mapped to physical page `0x20` while
it runs.

`software/z80neo/midiplayer/` is an SD-card **Standard MIDI File player** built
with the same target. It plays channel messages out UART1 (`0x90`) at 31250 8N1
— it selects baud code 10 on `0x92` and format `0x03` on `0x93`, overriding
`Z80NEO.INI`. Its file browser starts in `/MIDI` (root fallback) and walks the
card as a tree — directories are listed with a trailing `/`, a number opens a
file or descends, `..` goes up, pages of 20; `a`/`A` play all in the directory
(once/loop) and `r`/`R` play them in random order (once/loop), both recursing
depth-first through subdirectories (random shuffles each directory). `DIR_RESET`
scans whatever directory path is in the name buffer.
The Z80 cannot afford to parse SMF itself (an on-Z80 C parser cost 100–230 ms
per event, ~15x slow), so the **firmware parses**: `src/midi.c` opens the file
with `FILE_OPEN_MIDI` (command 8 on port `0xA0`), merges all `MTrk` by absolute
tick and streams grouped `(delay_ms, byte_count, raw MIDI bytes...)` records on
port `0xA2` (see `include/midi.h`): every channel message sharing a tick is
coalesced into one record, so the Z80 waits once per tick and then blits the raw
bytes from an assembly copy loop (`midi_play` in `midi_delay.asm`). The firmware
schedules every record against an **absolute playback clock** (RP2350
`time_us_64()`): each tick has a due time, and the delay it hands the Z80 is
recomputed as `due - now` on every record, so the Z80's replay cost is measured
and absorbed instead of compensated by a tuned constant (a long song cannot
drift, and the delay loop needs no recalibration). A gap longer than the 16-bit
millisecond field is split into delay-only records so bytes are never sent
early. Optional trims `MIDI_GROUP_X10` / `MIDI_BYTE_X10` (tenths of a ms,
subtracted from the delay) remain for deliberate bias and default to `0`. The
delay loop is calibrated for the ~128 kHz `CPU_SPEED` *including* the ~9.6 µs
per-access bus service (`ld de,2`). The program is ~5.4 KB. See its `README.md`.

While a MIDI file is open the **firmware clears the console and draws a
full-screen MIDI monitor** (`midi.c`, absolute VT102 cursor positioning): a
header with the file name, elapsed/total time, tempo (BPM) and track count, then
16 channel rows showing the current note, velocity bar, General MIDI instrument
name, volume and pan. The total time is pre-scanned at open, but only when the
whole file is in PSRAM (a streaming SD scan would stall the open for seconds).
The Z80 never parses or draws for it — the firmware already sees every message
as it builds records — so playback timing is untouched; a row is repainted only
when its state changes, throttled to `MIDI_PANEL_ROWS_PER_TICK` rows per
`MIDI_PANEL_INTERVAL_US`, and the header only when the elapsed second or tempo
changes. Events are applied at the record's due time (not when built), so the
monitor never runs ahead of the music, and the clock starts at the first due
record so a long opening gap is not counted. `Z80NEO.INI` `MIDI_PANEL=0`
disables it (raw, non-VT102 terminals). Both players get it automatically since
it lives in the shared firmware.

`software/cpm/midiplayer/` is the CP/M 2.2 twin (`MIDI.COM`, built with
`+cpm`); it shares the replay loop `software/z80neo/midiplayer/midi_delay.asm`
and talks to the same firmware ports, but drives the console through BDOS and
uses the shared `FILE_OPEN_MIDI` path. Because the file API and the CP/M disk
layer mount the same FatFS volume, `MIDI.COM` issues **`FILE_REMOUNT_CPM`
(command 9 on `0xA0`)** after playback; that drops `fileio`'s cached mount and
re-runs `cpm_disk_init()` so CCP disk I/O survives. See its `README.md`.

### CP/M 2.2 (primary target)

The CP/M 2.2 system lives in `software/cpm/z80neo/` (BIOS + CCP/BDOS patch +
boot vectors). Build it with:

```sh
cd software/cpm/z80neo
make                          # -> build/cpmz80neo.hex
```

The image is linked for a 60K CP/M: CCP/BDOS at `0xD400`, BIOS jump table at
`0xEB00` (see `z80neo.inc`). The firmware emulates **15** sector disks
(`src/cpm_disk.c`) on ports `0xE0` (data/command), `0xE1` (track), `0xE2`
(sector/status) and `0xE3` (drive select, 0=A). The images are
`CPMDISK0.IMG` … `CPMDISK14.IMG` (A:–O:) in the card root; drive geometry
(254 tracks, 26 sectors, 1K blocks, 128 dir entries, 2 reserved tracks) must
match `diskdefs`, or the BIOS DPB in `disk.asm` must be updated. The BIOS DPH
table has one entry per drive (`disk.asm`), and `disk_seldsk` writes the drive
number to port `0xE3`. The drive count lives in **three** places that must stay
in sync: `CPM_DRIVES` (`include/cpm_disk.h`), `DISK_DRIVES`
(`z80neo.inc`) and the 15-entry DPH table in `disk.asm`. The BIOS space
(0xEB00–0xFFFF) limits how many drives fit: each drive costs a 16-byte DPH plus
a 128-byte ALV and CSV.

`sdcard/Z80NEO.INI` currently boots `midiplayer.ihx` (the SD MIDI player); to
run CP/M instead, set `PROG1=cpmz80neo.hex` and copy it plus
`CPMDISK0.IMG`–`CPMDISK14.IMG` to the card root. The console (Z80 port `0x80`
and the firmware's own boot log) is on **UART0, 115200 8N1** (GPIO0/1), shared
by the firmware and CP/M.

### Software collection → disk images

`software/cpm/Collection/CPM_DISKS/<letter>/<number>/` is an extracted CP/M
software library (letters A–O, ~71 "disks" of host files). Two helpers build
z80neo images from it:

```sh
python3 tools/pack_cpm_collection.py --mode disk      # one image per <letter><number>
python3 tools/install_cpm_disks.py --list             # list available disks
python3 tools/install_cpm_disks.py A0 A1 B0 ...       # install up to 15 as CPMDISK0..14.IMG
```

`--mode letter` instead makes one image per letter with each number as a CP/M
user area, but letters A and C exceed one image that way, so `--mode disk` is
the default. The packer skips PDFs, non-8.3 names and (by default) everything
outside `.com/.sub/.txt/.doc`, and pads each image to the full geometry.

### CP/M disk images (TurBoCPMTools)

`tools/TBCPMTools/turbocpmtools.py` is a PySide6 GUI for the disk images. It
drives the installed `cpmtools` binaries (`cpmls`/`cpmcp`/`cpmrm`/`mkfs.cpm`)
with `software/cpm/z80neo/diskdefs`, so it reads/writes the same images as the
Windows `tools/CPMTG(English)/CpmtoolsGUI.exe`.

- Run it with `tools/TBCPMTools/run.sh [image]` (sources the bundled venv; the
  venv is Python 3.14 and uses the system PySide6 via
  `include-system-site-packages = true`).
- **Tools → Define format…** edits a geometry and appends it to the current
  `diskdefs`. New images are **padded to the full geometry**
  (`tracks*sectrk*seclen`), because `mkfs.cpm` only writes the reserved tracks
  and directory and lets added files grow the file — the Z80 CP/M disk layer
  needs every track addressable.
- Features: open / new (`mkfs`) image; list with per-file **user** column and
  disk **Used/Free** in the status bar; view (text/hex); extract (multi-select);
  add; import directory; **rename**; set **attributes** (`cpmchattr` r/s/a/1-4);
  delete (multi-select); drag & drop files/folders onto the window. A **User**
  spinbox (0–15) is the `user:file` argument for add/extract/delete/rename/attrs.
  A Properties panel shows the selected file's raw directory entry (user,
  extent, record count, allocation blocks, attributes) read from the image.
- **Tools → Space map…** paints the allocation blocks (directory / used / free)
  with a per-block owner tooltip.
- **Rename** edits the raw directory entry in place (`rename_in_image`) rather
  than extract/delete/add, so data, attributes and all extents are preserved
  (`cpmtools` has no rename command).
- `cpmtools` lists **all** user areas and displays names lower-cased; `-u`
  means "upper case", not user number. Add/extract/delete take `user:file`.
- The `diskdefs` **must match the image** (z80neo: `tracks 254`, `maxdir 128`);
  with a stale def (77/64) tools read past the 128-entry directory into file
  data and show garbage filenames. Keep `software/cpm/z80neo/diskdefs` and
  `tools/CPMTG(English)/diskdefs` in sync (both are correct now).

## Architecture

- **Bus capture (PIO)** — `src/z80bus_pio.pio` + `src/z80bus_pio.c`. Two PIO0
  state machines watch `MREQ` (GPIO20) and `IORQ` (GPIO22). On a cycle they
  assert `WAIT`, push a snapshot to the RX FIFO, and block on `PULL`. The ARM
  services the cycle from the main loop (`z80bus_pio_poll()`), reads the address
  and data through the `SEL1/2/3` mux, then writes the FIFO to release `WAIT`.
  Because the Z80 is stalled for the whole service time, timing is
  frequency-independent (just slow). `z80bus_pio_poll()` services the io SM
  first, then the mem SM.
- **Memory / MMU** — `src/memory.c`. Four 16 KB banks (`ram[4][16384]`) behind a
  4-page MMU selected on ports `0xF0`–`0xF3`. `mmu_read`/`mmu_write` map the
  16-bit address through `mmu_page[]`. Optional PSRAM extends banks.
- **I/O ports** — `io_read_port()` / `io_write_port()` in `memory.c`:
  `0x80`/`0x83` serial data, `0x81` serial status, `0x90`/`0x91` second UART
  (UART1) data/status, `0x92`/`0x93` UART1 baud/format config (write to set, read
  current), `0xF0`–`0xF3` MMU, `0xD1` I2C EEPROM, `0xE0`–`0xE2`
  CP/M disk + `0xE3` drive select (`src/cpm_disk.c`). The SD-card file API for
  bare-metal programs is `0xA0`–`0xA7` (`src/fileio.c`): `0xA0` command/status,
  `0xA1` filename (write = append, read = DIRNEXT bytes), `0xA2` data,
  `0xA3` name reset, `0xA4`–`0xA7` 32-bit seek argument / size result. The
  `DIR_RESET` command scans the directory named in the name buffer (empty =
  root); `DIRNEXT` returns every entry including subdirectories, flagged by
  `FILE_ST_DIR` (bit `0x40`) in the command status, so the MIDI players can walk
  the card as a tree (a number opens or descends, `..` goes up). The second UART is
  hardware **UART1 on the otherwise-free GPIO38 (TX) / GPIO39 (RX)**,
  initialised by `uart1_init()` and polled by `uart1_poll()`; `0x91` mirrors the
  console status bits (bit0 = RX ready, bit1 = TX ready). It is independent of
  the console UART0 and is not wired to the Fuzix serial interrupt.
  The **NullVDP SPI link** (`src/spi_vdp.c`, polled from the main loop) adds
  ports `0x94` (write = console byte to the FPGA VDP) and `0x95` (read = link
  status: bit0 TX ready, bit1 RX available, bit7 present). It is a software
   PIO SPI master (mode 0, acknowledged 32-bit frames with CRC-8,
   MISO/MOSI/SCK/CS = GPIO34/35/36/37) to the Tang Primer 20K NullVDP: every
   console byte drained from the TX ring is mirrored to it, and keyboard bytes
   it reports are injected into the console RX ring (`console_inject_rx`, which
   bypasses the local-echo filter). Invalid frames are retried and duplicate
   sequence numbers are suppressed by the FPGA, so a physical error cannot
   desynchronize the console parser. The link is enabled only if the slave
   response carries ID `0x4E` (`VDP_ID_VALUE`).
- **Clock / reset** — `src/z80clock.pio` generates the Z80 clock on **GPIO32**
  (`GPIO_PWM_SIG`); the PIO output pin is a parameter so the define is honoured.
  `CPU_SPEED` in `main.c` is the frequency and `CPU_DUTY` is the HIGH-time
  fraction; both default to 128 kHz / 50% and are overridable at boot from
  `Z80NEO.INI` (`Z80_KHZ` 32..1000, `Z80_DUTY` 5..95 %), applied by re-configuring
  the clock SM after the INI is parsed (before the Z80 reset is released). The
  period is
  the SM clock divider and the high/low split is a pair of X/Y loop counts
  loaded into the program once, so frequency and duty are independent (~0.4%
  duty resolution down to ~600 kHz; fewer steps as the frequency rises). Duty is
  clamped to 5%..95% so the clock can never stop. The Z80 **RESET** is driven on
  **GPIO33** (`RESET_OUT`). Both are RP2350B-only pins (GPIO30-47), so the
  firmware must be built for the BB48 board (see Build).
- **Display / keys** — `src/display.c` runs on core 1 (`multicore_launch_core1`)
  and drives the SSD1306 over I2C1 (`src/ssd1306_i2c.c`). Core 0 owns the bus
  loop and UART. The ADC keypad is debounced and, if it does not rest at `NONE`
  at boot, button handling is disabled so a floating ADC cannot reset the Z80 in
  a   loop; the menu is the *only* display UI (the old ON/OFF monitor screens are
  gone). `UP`/`DOWN` navigate, `OK` opens/acts, `CANCEL`/`BACK` returns. The Z80
  keeps running while the menu is open (pages read live globals and refresh
  every `UI_TICK` loops); destructive actions (bank select/clear, file
  load/browse) pause it with the `disabled`/`confirmed` handshake first, and the
  Files page re-mounts the CP/M images (`cpm_disk_init`) after any SD access
  because its own `f_mount` swaps the volume under the running Z80. The
  **Screensaver** page toggles the idle saver, cycles the demo and idle timeout,
  and offers a manual preview; `src/screensaver.c` holds the demos (Puzzle,
  Snake, Starfield, Life, Bounce) and their settings, seeded from `Z80NEO.INI`.
- **Console RX** — the firmware suppresses terminal local echo: RX bytes that
  match a recently transmitted byte within ~30 ms are dropped (`memory.c`), so
  the CCP does not read its own output back as input.
- **Boot** — `main.c` mounts SD, reads `Z80NEO.INI` (`sd_read_init`), loads the
  Intel HEX program (`load_file`) into banks, holds the Z80 in reset, then
  starts the PIO handler and releases reset.
- **Hard fault** — `src/hardfault.c` overrides `isr_hardfault` and dumps fault
  registers + stack frame to UART0.

### `Z80NEO.INI` format (SD card)

Preferred format is named `KEY=VALUE` lines (also accepts `KEY:VALUE` or
whitespace as separator; keys are case-insensitive; `#`/`;` comment and blank
lines are ignored; order-independent; unknown keys are ignored). Keys:

| Key | Meaning |
|-----|---------|
| `MACHINE` | machine name (`MACHINE[FILE_LENGTH]`) |
| `CANCEL2`, `CANCEL`, `OK`, `BACK`, `DOWN`, `UP` | ADC thresholds, 3 hex digits each |
| `PROG1`..`PROG4` | program filenames (`BANK_PROG[0..3]`); only `PROG1` is loaded at startup |
| `DEBUG` | `1` = hold in a busy loop at boot |
| `FUZIX` | `F`/`f`/`1` = full-RAM load + hardware `/INT` |
| `BAUD` | UART0 console baud (`UART_BAUD` in `src/memory.c`, default `BAUD_RATE` = 115200) |
| `UART1_BAUD` | UART1 baud (`UART1_BAUD`, default 115200); any 300..3000000, but `0x92` config only exposes the 10 standard codes |
| `UART1_FORMAT` | UART1 line format, `8N1`-style: data bits 5-8, parity N/E/O, stop 1/2 (default `8N1`) |
| `Z80_KHZ` | Z80 clock in kHz, 32..1000 (default `128`). Raise in steps and check for corruption; the NMOS Z80 samples WAIT relative to the clock |
| `Z80_DUTY` | Z80 clock HIGH-time duty in %, 5..95 (default `50`) |
| `MIDI_GROUP_X10`, `MIDI_BYTE_X10` | MIDI player optional speed trim, tenths of a ms subtracted from each record's delay (defaults `0` / `0`); raise to play faster. The absolute scheduler makes this unnecessary in normal use |
| `MIDI_PANEL` | `1`/`0` (or `ON`/`OFF`) = firmware console channel monitor while a MIDI file plays (default `1`); set `0` for a raw, non-VT102 terminal |
| `SCREENSAVER` | `1`/`0` (or `ON`/`OFF`) = idle OLED screensaver enabled (default `1`) |
| `SS_MODE` | screensaver demo: `Puzzle` (default), `Snake`, `Starfield`, `Life`, `Bounce` (index `0`..`4` also accepted) |
| `SS_IDLE` | idle seconds before the screensaver starts, 5..3600 (default `30`) |

The old positional layout is still accepted as a fallback: machine name, the six
ADC thresholds, four filenames, then optional trailing lines (first non-`BAUD`
line = ADC-debug flag, second = FUZIX flag; a `BAUD <n>` line may appear anywhere
among them). The layout is auto-detected from whether the first non-comment line
contains `=` or `:`. See `sdcard/Z80NEO.INI`.

Program images are Intel HEX. Both the `.hex` (CP/M) and `.ihx` (Fuzix/z80dk)
spellings are accepted by the loader, and the Files menu lists both
(`is_prog_file()` in `src/memory.c`); the filenames in `BANK_PROG[]` are used
verbatim, so either works there too.

## Conventions

- C11 / C++17. Match the existing style: **tabs** for indentation, 4-wide
  (`.clang-format`, LLVM-based). Format with `clang-format` before committing
  C/C++ changes.
- Do **not** add comments unless they explain non-obvious hardware timing or
  workarounds; the existing comments are intentionally dense around bus timing.
- Public symbols are declared in `include/*.h`; keep the extern/global style.
- Firmware is a single executable target; add new `.c` files to
  `firmware/z80neo/CMakeLists.txt`.
- `.gitignore` excludes `software/` and all `*.hex`/`*.bin`/`*.o`; do not force
  add build artifacts.
- Hardware is CERN-OHL-S-2.0; software is GPLv3; docs CC BY-SA 4.0.

## Gotchas

- **PIO `WAIT` instruction is broken in this SDK's pioasm** — `z80bus_pio.pio`
  deliberately uses `JMP PIN` polling loops instead. Do not "simplify" it to
  `WAIT`.
- **Bus settle constants** — `BUS_DATA_SETTLE_US`, `BUS_ADDR_LO/HI_SETTLE_US`
  and `BUS_IO_PORT_SETTLE_US` in `z80bus_pio.c` dominate Z80 throughput (sum
  per memory access). They are hidden behind `WAIT` (the Z80 is stalled the
  whole time) and are a property of the analog bus, not of `CPU_SPEED`. The
  original values (20/10/40/40 µs, tuned for an older board) were cut to
  1/1/2/2 µs, which proved **marginal on this board**: the address mux can
  still be slewing when `bus_read_stable()` samples, so an operand fetch came
  back from the previous address, the Z80 jumped into the signon string and
  echoed a `^@` flood. 3/3/3/3 µs is **validated on the current board** (clean
  CP/M). Raise `DATA` first if data-bit errors reappear, then the address/IO
  values. Change with care and re-measure.
- **Shared Z80 WAIT net**: the mem SM drives GPIO5 and the io SM drives GPIO4;
  both are wired to the same Z80 `WAIT` pin. The pins are driven **open-drain**
  (PIO drives LOW to assert, high-Z to release) with pull-ups, so the two SMs
  can never fight. Do not revert this to push-pull (`SET PINS,1`).
- `build.sh` and `export.sh` contain absolute local paths (`/home/turboss/...`)
  and will need editing on another machine.
- **The board is RP2350B (48 GPIOs), not RP2350A.** `firmware/boards/z80neo_bb48.h`
  sets `PICO_RP2350A 0` and is selected via `PICO_BOARD=z80neo_bb48` in both
  `firmware/CMakeLists.txt` and `build.sh`. Building for `pico2` (A, 30 GPIOs)
  silently disables GPIO30-47 — including the Z80 clock (GPIO32), reset
  (GPIO33) and the ADC keypad (GPIO40). `main.c` `#error`s on the A package.
- **GPIO32 = Z80 clock, GPIO33 = Z80 reset** (`GPIO_PWM_SIG` / `RESET_OUT` in
  `include/memory.h`). Both need the RP2350B build above.
- **RP2350B PIO addresses only 32 pins per instance** (0-31 or 16-47). When a
  PIO-driven pin is ≥32 (the clock on GPIO32), call `pio_set_gpio_base(pio, 16)`
  *before* `pio_add_program`/`pio_sm_init` — `pio_sm_set_config` does not set the
  GPIO base for you, and without it the SET base wraps and the SM drives the
  wrong pin (e.g. GPIO0). See the clock init in `main.c`.

## VoidVDP (VersaTerm fork, RP2350 VGA terminal)

`vdp/` is **VoidVDP**, turboss's fork of
[VersaTerm](https://github.com/dhansel/VersaTerm), used as a standalone **VGA**
serial terminal (CMake target `VoidVDP`). It builds for
RP2040 (DVI + VGA) and RP2350 / Pico 2 (VGA) — it is a **separate board**, not
the z80neo; do not flash it to the z80neo. Full build lines are in
`vdp/ReadMe.md`. Builds use the system SDK `~/Dev/PICO/pico-sdk` (2.3.0), set in
`vdp/build.sh` and the `CMakeLists.txt` default; the bundled `lib/pico-sdk` and
`lib/Pico-PIO-USB` submodules were removed (`lib/` holds only `PicoDVI` +
`PicoVGA`).

It talks to the host over **UART1 on GP20 (TX) / GP21 (RX), default 115200 8N1**
(`vdp/src/pins.h`; other pins: VGA 0–8, buzzer 9, PS/2 10/11, DVI 12–19, LED 25,
HDMI detect 28). The keyboard is a **PS/2** one on **GP11 = DATA / GP10 = CLOCK**
(`PS2_KEYBOARD=ON`, the default; `keyboard_ps2.c`). `PS2_KEYBOARD` **never starts
the USB host controller**, so only the USB keyboard host is disabled and the
native port stays a CDC device (its `USB port mode` default becomes **Device**).
`-DPS2_KEYBOARD=OFF` restores the native-port USB keyboard, whose role default is
then **Host** (`settings.USB.mode`, `config.c`), *not* upstream's Auto-detect: on
a Pico 2 GPIO24 is `PICO_VBUS_PIN`, so an OTG adapter that presents VBUS makes
Auto-detect choose Device and the keyboard is never hosted. `-DUSB_HOST_PIO=ON`
selects a GPIO USB-host keyboard through the SDK's Pico-PIO-USB instead; it
shares GP10/GP11 with PS/2, so it and `PS2_KEYBOARD` are mutually exclusive.
The default **display type** is **VGA** (not upstream Auto-detect:
`framebuf_init` reads GPIO28 `PIN_HDMI_DETECT`, which floats on this board and
can read as DVI). `CONFIG_VERSION` in `config.c` is bumped when a default
changes so a stale flash config is rejected and rewritten.

### RP2350 bring-up gotchas (all fixed in-tree)

- **The GPIO coprocessor hard-faults.** `gpio_put()` uses the RP2350 GPIO
  coprocessor (`mcr`) by default and faults with `UFSR NOCP`. The symptom is
  severe: the first received serial character calls `blink_led()` →
  `gpio_put()` → core 0 dies in `isr_hardfault`, so serial input never displays.
  Set **`PICO_USE_GPIO_COPROCESSOR=0`** (`vdp/src/CMakeLists.txt`), same as the
  firmware.
- **tinyusb task calls block forever.** pico-sdk 2.3.0's `tud_task()` /
  `tuh_task()` wrappers pass `UINT32_MAX`, so they block when no USB event is
  pending and starve the rest of `run_tasks()` (serial, keyboard). Use
  `tud_task_ext(0, false)` / `tuh_task_ext(0, false)`.
- **The FPU is disabled at boot.** The RP2350 boot ROM RCP setup leaves CPACR
  CP10/CP11 clear while the build is `-march=armv8-m.main+fp` (softfp), so VFP
  traps with NOCP. Call `enable_fpu()` on **both** cores (`main.c`,
  `framebuf_vga.cpp`).
- **The VGA PLL never locks.** `lib/PicoVGA/util/overclock.cpp` hard-coded the
  RP2040 VCO range (400 MHz min); RP2350 needs ≥ 750 MHz. Use the SDK's
  `PICO_PLL_VCO_MIN/MAX_FREQ_HZ`. A bad VCO shows as VGA sync appearing briefly
  and then dying (looks like the monitor power-cycling).
- **RP2350 has no SIO hardware divider** (`SIO_DIV_*`); `render/vga_ctext.S`
  uses the M33 `udiv`/`mls` path (RP2040 path kept behind `PICO_RP2350`).
- **Default core-0 stack (2 K) is too small** for tinyusb + PicoVGA;
  `PICO_STACK_SIZE=0xC00`.
- The PicoVGA display libs are RP2040-oriented: unused renderers are stubbed in
  `lib/PicoVGA/render/render_stubs.S` and RP2040-only reg headers are guarded in
  `lib/PicoVGA/global.h`.

## NullVDP (Tang Primer 20K FPGA VDP)

`vdp/NullVDP/` is a **Verilog** VDP for the z80neo on a Sipeed **Tang Primer
20K** (GW2A-LV18PG256C8/I7) + Dock, built with **oss-cad-suite**
(`~/Apps/oss-cad-suite`; yosys → nextpnr-himbaechel → gowin_pack →
openFPGALoader), HDMI 640×480 output.  It is a fork of the local
`tangprimer20k-oss-examples/dummy_terminal` example and is being grown into a
hardware implementation of the same console protocol the RP2350 VoidVDP speaks
(`doc/vdp_protocol.md`, `common/z80neo_protocol.h`), driven over **SPI** instead
of UART.  Build/program/sim: `./build.sh [program|sim]`; extra sims include
`make sim-gfx` (the `ESC g` path).  Phase 2 (colour text:
per-cell fg/bg + 16-colour palette, SGR, cursor/erase, hardware scroll, input
FIFO) is done and covered by `sim/tb_terminal.v`.
Phase 3 (**SPI** — `src/spi_slave.v` plus the firmware master `src/spi_vdp.c`
and Z80 ports `0x94`/`0x95`) is done: SPI console bytes (highest priority) feed
the terminal and PS/2 keyboard bytes are pushed to the host RX FIFO; see
`doc/nullvdp_spi_protocol.md`.  The link is on **Dock PMOD03** (SCK N6, MOSI A11,
MISO B11, CS D11; Pico GPIO36/35/34/37).  It **must not** use bank-0/1 balls
(e.g. PMOD02's D14 = `1/IOT44A`): the HDMI TMDS runs over true-LVDS, which forces
banks 0/1 into LVDS mode, so those balls cannot do single-ended LVCMOS while HDMI
is active and the SPI clock silently dies.
Phase 4 (**`ESC g` bitmap layer**) is **done**: `src/gfx_buffer.v` holds three
1-bit 640×256 bitplanes (30 BSRAM blocks; the whole design is 39/46, ~19% LUT),
`src/gfx_engine.v` draws pixel/Bresenham-line/fill/clear, `src/terminal_handler.v`
collects each command payload and stalls the console until the engine is done,
and `src/text_controller.v` shows the layer centred (112-line bars) instead of
the text grid while active (the F12 menu still overlays).  It is **8 colours**
(3bpp — the low 3 bits of a colour byte pick palette 0–7); 16-colour 4bpp would
need ~36 blocks and does not fit BSRAM next to the text/overlay buffers.  Text is
not rendered into the bitmap (the text grid is kept and reappears on mode off).
Because the layer speaks the shared protocol, the z88dk `+z80neo` graphics API
(`software/z88dk/libsrc/target/z80neo/gfx/z80neo_gfx.c`) and
`software/z80neo/demos/` run on the NullVDP unchanged (colours 8–15 map to 0–7);
`make sim-gfx` covers it (`sim/tb_gfx.v`).
The F12 options menu sets `cursor_en` / `local_echo` / `pal_bright`.
**Local echo** is three-state **AUTO / ON / OFF** (default AUTO): AUTO echoes
PS/2 keys locally only when the z80neo host is *not* driving the display, ON
forces local echo even while the host polls the SPI link, OFF never does.
`host_active` (any SPI traffic within ~0.15 s) alone could not be overridden
because the firmware polls the link continuously, so the menu toggle had no
effect while the Pico was attached — set ON to type locally.  See
`vdp/NullVDP/README.md` for the roadmap.

## sdl_client (PC SDL3 VT100 terminal)

`sdl_client/` is a **PC-side** terminal, not firmware: it opens a serial port
(`/dev/ttyACM0` by default, the debug probe bridging the z80neo UART0 console)
and emulates a **VT100/VT102** in an SDL3 window, so the Z80 can be driven from
a desktop instead of the on-board VDP. Build with `sdl_client/build.sh`
(CMake + `pkg-config sdl3`, output `build/z80neo_client`). It is not built by
the Pico toolchain and has no SD/flash interaction.

- `src/terminal.c` is a self-contained VT102 parser + 80x24 cell grid
  (cursor motion, erase, insert/delete, scroll regions, SGR, DEC special
  graphics, VT52 mode, device/status/cursor reports). Input bytes
  `0x80`-`0xFF` decode as **CP437**. It also implements the VDP `ESC g`
  bitmap protocol as a 640x256 true-colour layer, one 4-bit palette index per
  pixel; while bitmap mode is active that layer is rendered instead of the text
  grid.
- `src/render.c` blits either the text grid (8x16 console font,
  `src/font8x16.h`, generated by `tools/gen_font.py` from the Linux console
  `default8x16` PSF2 font) or the bitmap layer into an ARGB texture scaled by
  an integer factor; the bitmap is centred vertically like the VDP.
- `src/serial.c` is a raw POSIX/termios non-blocking port; `src/main.c` wires
  SDL events, key/paste mapping, reconnect and the render loop.
- The console protocol is specified in `doc/vdp_protocol.md`; the `ESC g`
  command bytes/payload lengths come from `common/z80neo_protocol.h`, which
  both `vdp/src/terminal.c` and `sdl_client/src/terminal.c` include. Keep the
  two implementations in step with that spec (VDP defaults: Enter=CR, BS=0x08,
  DEL=0x7f, CR->col 0, LF->down one); section 4 lists the intentional
  differences (PETSCII, double-size, text-into-bitmap, RP2040 colour).

## Known review findings

Not yet fixed:

1. MMU `phys_to_bank()` aliases the advertised 32 physical pages onto only 4
   banks (`MAX_BANKS`); PSRAM banks are unreachable via the MMU.
2. Per-access settle delays cap effective Z80 *throughput* to tens of kHz.
   3/3/3/3 µs is validated on the current board (1/1/2/2 was marginal and
   caused `^@`/warm-boot corruption). Raise them in `z80bus_pio.c` if data-bit
   errors appear.
3. **The Z80 clock is limited by the WAIT round-trip.** The NMOS Z0840004
   samples `WAIT` before the Pico has asserted it, so above the stable point it
   reads garbage and warm-boots. With the clock buffered to 5 V
   (GPIO32 → 74HCT245), **~128 kHz is stable on this board**; 256 kHz derails
   (garbled `dir`, `^@` flood, warm-boot loop). The NMOS clock input also
   wants a ~5 V swing. Raise `CPU_SPEED` only with a faster WAIT path / bus
   redesign, and verify against the UART boot log and a repeated `dir`.

Fixed:

- The post-boot `^@` flood: CP/M's BDOS echoes a NUL from the console as `^@`.
  It was caused by the Z80 sampling the console data port (`0x80`) ahead of
  `CONSTAT`, returning a phantom 0; the root trigger was the marginal bus
  settle/clock (see Known findings 2–3). The console status port now derives
  "ready" from the RX ring itself (never a stale `rx_data_available`), and the
  settle times / clock were raised to the stable point.
- Z80 refresh cycles (MREQ active with neither RD nor WR) are skipped instead
  of being serviced as memory reads (`handle_mem` waits for RD/WR or MREQ end
  before sampling the address).
- The shared WAIT net is driven open-drain (see Gotchas).
- `PICO_USE_GPIO_COPROCESSOR=0` — the RP2350 GPIO coprocessor (`mcr p0`)
  hard-faulted with `UFSR NOCP` on this SDK/core combination.
- Spurious ADC keypad reads no longer reset the Z80 in a loop: the trigger is
  debounced and button handling is disabled unless the keypad rests at `NONE`
  at boot (`display.c`). The menu debounce is non-blocking — `read_button_state_debounced()`
  samples at most once per `BUTTON_DEBOUNCE_US` (1 ms) and accepts a state after
  `BUTTON_DEBOUNCE_SAMPLES` (4) identical reads, instead of the old `sleep_ms(10)`.
- Terminal local echo is suppressed by dropping RX bytes that match a recently
  transmitted byte within ~30 ms (`memory.c`), instead of the old fragile
  score-based filter.
- `mmu_write` write-protects only the `0x0000` opcode byte, so the BIOS can
  install the warm-boot vector (`JP WBOOT`) at `0x0001`/`0x0002`.
- CP/M BIOS DPB geometry matches the image (`DRM=127`, `DSM=818`, `AL0=0xF0`,
  `CKS=32`) instead of the stale 64-entry/504-block values.
- CP/M disk is emulated as four drives A:–D: (`CPMDISK0..3.IMG`, drive select
  port `0xE3`).
- Board builds as RP2350B/BB48 via `firmware/boards/z80neo_bb48.h`; the
  pico_fatfs hardcoded `PICO_RP2350A` redefinition is guarded.
