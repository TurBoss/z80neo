# z80neo serial bootloader

A standalone Z80 bootloader for the z80neo, built with the in-tree z88dk
`+z80neo` target.  It links at `0x0000`, owns page 0, and talks to the console
on ports `0x80` (data) / `0x81` (status).  It receives a program over UART0 as
**Intel HEX** (or the simple `@addr byte...` form), loads it into Z80 memory and
jumps to the entry point.  The PC-side companion is
[`tools/z80neo_loader`](../../../tools/z80neo_loader/) (PySide6 GUI), and the
older `software/asm/z80-loader.py` speaks the same protocol.

## Build

```sh
make                 # -> build/zload.ihx  (Intel HEX for Z80NEO.INI)
make bin             # -> build/zload      (raw binary)
make Z88DK=/path/to/z88dk
```

`make` uses `../../z88dk` by default.

## Deploy

Copy `build/zload.ihx` to the SD-card root and point the first program in
`sdcard/Z80NEO.INI` at it:

```ini
PROG1=zload.ihx
```

Reset the board.  The banner appears on UART0 (115200 8N1):

```
z80neo serial loader
send Intel HEX or @addr records; EOF/blank runs
ld>
```

## Protocol

One record per line, terminated by `\n` (a lone `\r` is also accepted).
After **every** line the loader emits a single `.` (0x2E) as a pacing ACK,
preceded by error text on failure:

| Line | Meaning |
|------|---------|
| `:LLAAAATT…CC` | Intel HEX record (types 00/01/02/03/04/05) |
| `@AAAAAAAA BB BB …` | raw block: address then whitespace-separated bytes |
| empty line | end of transmission, run |
| `:` `:00000001FF` | Intel EOF record, run |

A bad Intel checksum prints `!chk AAAA` before the ACK and the record is
skipped.

**The host must wait for the `.` before sending the next line.**  The NMOS Z80
at 128 kHz cannot keep up with unpaced 115200 baud, and the firmware's console
RX ring is only 256 bytes; back-to-back sends would overflow it.

## Where programs load

The loader keeps itself in page 0, so it must not be overwritten while it runs.
Two paths:

- **`0x4000`–`0xFFFF` (MMU pages 1–3)** — written in place, then entered with
  `SP = 0xFFFF`.  Build these with `-DCRT_ORG_CODE=0x4000`.
- **`0x0000`–`0x3FFF` (page 0)** — staged at `0x4000 + addr`, then a
  position-independent copier running from page 2 (`0x8000`) copies the image
  down over page 0 and enters it.  This is the normal `+z80neo` link address
  (e.g. `fib.hex`, `bootloader/opcodes.hex`).

An image that mixes both regions is refused (`!mixed …`); split it and load the
page-0 half first or rebuild it for `0x4000`.

The page-0 copier remaps MMU page 0 to register value `0x34`, which aliases
bank 0 (the same physical bank the firmware's default `0x20` selects) while
dodging the firmware's reset-vector write-protect, which matches the value
`0x20` exactly.  It needs `MAX_BANKS = 20` in `firmware/z80neo/include/memory.h`
(`(0x34 - 0x20) % 20 == 0`); if that constant changes, update the copier.

Once the loaded program starts, the loader is gone.  Reset the board to get it
back.

## Host formats

| Format | Detected by | Notes |
|--------|-------------|-------|
| Intel HEX | first non-blank char `:` | full record checksum verified |
| `@addr …` | first non-blank char `@` | matches `software/asm/bootloader/opcodes.hex` |

Example (`opcodes.hex`, a 5-byte page-0 program):

```
@00000000 3E AA D3 40 76
:00000001FF
```

## Files

| Path | Purpose |
|------|---------|
| `src/loader.c` | record parsing, load-path selection, console I/O |
| `src/loader_asm.asm` | `loader_start` / `loader_relocate` and the relocatable copier |
| `Makefile` | zcc `+z80neo` build |
