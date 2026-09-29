# Z80NEO Loader

A PySide6 GUI that pushes a program to the z80neo over the console UART and has
the on-board **serial bootloader** run it.  It replaces
`software/asm/z80-loader.py` with a real front-end: port picker, parse preview,
paced transfer, progress bar and a live console.

## Setup

Needs **PySide6** and **pyserial**:

```sh
python3 -m venv venv
venv/bin/pip install PySide6 pyserial
./run.sh                 # or: python3 z80neo_loader.py [file]
```

`run.sh` uses `venv/` if present, otherwise the system `python3`.

## Host side

1. Flash the bootloader: build `software/z80neo/loader` and copy `zload.ihx` to
   the SD-card root, set `PROG1=zload.ihx` in `sdcard/Z80NEO.INI`, then reset.
   The banner and `ld>` prompt appear on UART0 (115200 8N1).
2. Launch the GUI, pick the port (`/dev/ttyACM0` by default), drop in a
   `.hex`/`.ihx` file (or a raw `.bin` with a base address), and click
   **Load & Run**.

The GUI shows the parsed image (format, byte count, address range, entry
point), sends one record per line and waits for the loader's `.` ACK before the
next, so the 128 kHz Z80 is never overrun.  After the last record it keeps
reading the console so the program's own output is visible.

An image that mixes page-0 (`0x0000`-`0x3FFF`) and `0x4000+` addresses is
refused with a warning — the bootloader cannot relocate that; relink for one
region.

## Files

| Path | Purpose |
|------|---------|
| `z80neo_loader.py` | the application (single file) |
| `run.sh` | launcher (venv or system Python) |

The wire protocol is documented in
[`software/z80neo/loader/README.md`](../../software/z80neo/loader/README.md).
