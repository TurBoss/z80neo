# z80neo monitor / debugger

A standalone Z80 machine-language monitor for the z80neo, built with the
in-tree z88dk `+z80neo` target.  It links at `0x0000`, owns page 0, and talks
to the console on ports `0x80` (data) / `0x81` (status).  There is no CP/M and
no OS underneath it: the Pico firmware loads the Intel HEX and releases reset,
and the monitor is the program.

## Features

- Memory dump (`D`), examine/edit (`E`), fill (`F`), move (`M`), compare (`C`).
- Disassembler (`U`) with a full instruction-length calculator.
- Register display/edit (`R`), including AF', BC', DE', HL', IX, IY, SP, PC, I, R.
- Run (`G`), single-step (`T`), and up to 8 software breakpoints (`B`).
- Port input/output (`I` / `O`) and MMU page control (`P`).
- Intel HEX loader over the console (`L`).

## Build

```sh
make                 # -> build/zmon.ihx  (Intel HEX for Z80NEO.INI)
make bin             # -> build/zmon      (raw binary)
make Z88DK=/path/to/z88dk
```

`make` uses `../../z88dk` by default.

## Deploy

1. Copy `build/zmon.ihx` to the SD-card root.
2. Point the first program in `sdcard/Z80NEO.INI` at it
   (`BANK_PROG[0]`, the first filename after the six ADC thresholds):

   ```
   cpmz80neo.hex   ->   zmon.ihx
   ```
3. Reset the board.  The prompt appears on UART0 (115200 8N1).

## Commands

| Command | Action |
|---------|--------|
| `H` or `?` | help |
| `D [start [end]]` | dump memory; no args continues from the last address |
| `E addr` | examine/edit; Enter keeps and advances, `-` goes back, `.` quits |
| `F start end byte` | fill an inclusive range |
| `M start end dest` | move/copy, overlap-safe |
| `C start end dest` | compare and list differences |
| `U [addr]` | disassemble 8 instructions from `addr` (continues with no arg) |
| `R [reg val]` | show registers, or set one (`af bc de hl af' bc' de' hl' ix iy sp pc i r`) |
| `G [addr]` | run from `addr` (or the current PC) |
| `T [n]` | single-step `n` instructions (default 1) |
| `B` | list breakpoints |
| `B addr` | set a breakpoint |
| `B -addr` | clear a breakpoint |
| `B *` | clear all breakpoints |
| `I port` | read a byte from an I/O port |
| `O port byte` | write a byte to an I/O port |
| `P [p1 p2 p3]` | show/set MMU pages 1–3 (page 0 is the monitor) |
| `L` | load Intel HEX from the console |

## Memory model

The Pico firmware's HEX loader only fills `0x0000-0x3FFF`, so the monitor
code, data and BSS all live in page 0.  On reset it prints the first free
page-0 byte (`free RAM from 0xXXXX`), which is the end of its BSS.  Programs
under test normally build for `0x4000` and up (MMU pages 1–3), which the
monitor leaves entirely free:

```sh
zcc +z80neo -subtype=bin -DCRT_ORG_CODE=0x4000 prog.c -o prog
```

Mini-assembler programs can also use the small page-0 gap between
`free_base` and `0x3FFF`.  The Intel HEX loader (`L`) refuses to write below
`free_base`, so it can never silently overwrite the monitor.

The user stack defaults to `0x7FFF` (top of page 1); change it with
`R sp xxxx` before `G`.

## How breakpoints and stepping work

The target has no source of interrupts, so breakpoints are software traps:
before running, the monitor writes `0xFF` (RST 38h) at each breakpoint and at
the step target.  The `+z80neo` CRT installs the RST 38h vector at `0x0038`;
`src/mon_asm.asm` overrides the library handler with one that snapshots every
register into `_mon_regs`, restores the monitor stack, and returns to the
monitor.  Breakpoint bytes are restored before the prompt returns, so dumps
and disassembly always show real code.

Caveats:

- **`RST 38h` is reserved by the monitor.**  A program that executes `RST 38h`
  itself simply traps back to the prompt.
- **Page 0 must stay mapped to `0x20`** while the monitor is alive; use `P`
  only for pages 1–3.
- Stepping patches `PC + instruction-length`; if the instruction being
  stepped is a taken jump/call/return, execution may instead stop at a
  breakpoint or keep running.

## Files

| Path | Purpose |
|------|---------|
| `src/monitor.c` | command loop, memory/IO/MMU commands, breakpoints, HEX loader |
| `src/disasm.c` | instruction length + disassembler |
| `src/mon_asm.asm` | entry point, dedicated stack, RST 38h trap, `mon_run` |
| `src/monitor.h` | shared definitions between C and asm |
