# turboterm — CP/M UART1 terminal client

A CP/M 2.2 transient (`.COM`) for the z80neo built with z88dk's `+cpm` target.
It bridges the **CP/M console** (the VDP, or the PC `sdl_client`) to the second
UART the firmware exposes:

```
CP/M console input   ->  UART1 TX (0x90 write)
UART1 RX (0x90 read) ->  CP/M console output
```

It lets you drive a device wired to **UART1 (GPIO38 = TX, GPIO39 = RX)** from
the normal CP/M prompt.

## Ports

| Port | Direction | Meaning |
|------|-----------|---------|
| `0x90` | out | UART1 data |
| `0x90` | in  | UART1 data (RX) |
| `0x91` | in  | UART1 status: bit0 = RX ready, bit1 = TX ready |
| `0x92` | write | set UART1 baud (code); read back returns the current code |
| `0x93` | write | set UART1 line format; read back returns the current format |

`0x92`/`0x93` are the firmware config ports. The baud code indexes
`115200 57600 38400 19200 9600 4800 2400 1200 600 300` (0..9). The format byte
packs data bits as `db-5` in bits[1:0], 2 stop bits as bit3, and parity
(`0` none / `1` even / `2` odd) in bits[5:4]; bit7 is set on a read to mark a
firmware that implements the port. Write these to retune the hardware UART1; the
setting persists until reset and is shared by every program.

The console side uses CP/M BDOS function 6 (direct console I/O), so it works
under any console the BIOS drives.

## Options

```
TTERM [-b BAUD] [-f FMT] [-?]
  -b  baud: 115200 57600 38400 19200 9600 4800 2400 1200 600 300
  -f  line format, e.g. 8N1 or 7E2 (data 5-8, parity N/E/O, stop 1-2)
  -?  help
```

A bare `19200` is taken as the baud, a bare `8N1` as the format; `/` works in
place of `-`. With no arguments the current settings (the `Z80NEO.INI` defaults)
are left alone and shown. Examples:

```
A>TTERM 19200          (bare baud)
A>TTERM -b 9600 -f 7E1
A>TTERM /f8N1
```

Baud and format changes need firmware with the `0x92`/`0x93` config ports;
on older firmware the writes are ignored (the bridge still runs at 115200 8N1).

## Build

```sh
make                 # -> build/TTERM.COM
make NAME=FOO        # -> build/FOO.COM (any valid 8.3 stem)
make Z88DK=/path/to/z88dk
```

`make` uses the in-tree `../../z88dk` by default (the same toolchain as the
`+z80neo` target). Output is a CP/M transient that loads at `0x0100`. The
project directory is `turboterm`, but CP/M names are 8.3, so the executable is
**`TTERM.COM`** (override with `NAME=`).

## Install and run

This is a CP/M `.COM`, **not** a `Z80NEO.INI` program: copy it into a CP/M disk
image instead of the card root. `cpmtools` finds `diskdefs` in the current
directory, so run it from `software/cpm/z80neo/` (which holds the `z80neo`
geometry):

```sh
cd software/cpm/z80neo
cpmcp -f z80neo ../../../sdcard/CPMDISK0.IMG ../turboterm/build/TTERM.COM 0:TTERM.COM
```

The `TBCPMTools` GUI (`tools/TBCPMTools/run.sh`) can also add the file by
drag & drop.

At the CP/M prompt:

```
A>TTERM
```

## Notes

- **Ctrl-] (0x1D) exits** back to CP/M.
- The bridge is byte-transparent; the remote device's own echo is what you see.
  Run the console terminal with **local echo OFF** (the VDP and `sdl_client`
  already are).
- UART1 bytes are passed through unmasked, so 8-bit data (e.g. PETSCII) reaches
  the console intact.
- The RP2350 pads are 3.3 V. A 5 V TTL or RS-232 device on UART1 needs level
  shifting (RS-232 also needs a transceiver).
- Jumper EXT2 pin 18 (GPIO38) to pin 19 (GPIO39) for a loopback self-test:
  every typed character is echoed back once.
