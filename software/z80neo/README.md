# z80neo C example

A minimal bare-metal C program for the z80neo, built with the in-tree z88dk
target `+z80neo` (see the repository `AGENTS.md`, section *C programs
(`zcc +z80neo`)*).

The program links at `ORG 0x0000`, runs directly on the Z80 (no CP/M), and
talks to the console on ports `0x80` (data) / `0x81` (status).  It prints a
small banner, reads the console status port, runs a tiny sieve, then echoes
key codes until `q` / `ESC`.

## Build

```sh
make                 # -> build/demo.ihx  (Intel HEX)
make bin             # -> build/demo      (raw binary)
make Z88DK=/path/to/z88dk
```

`make` uses `../z88dk` by default; it only needs `bin/zcc` and
`lib/config` (i.e. `ZCCCFG`).

## Run on the z80neo

1. Copy `build/demo.ihx` to the SD-card root.
2. Set the first program in `Z80NEO.INI`:

   ```
   cpmz80neo.hex   ->   demo.ihx
   ```

   (`BANK_PROG[0]`, the first filename after the six ADC thresholds.)
3. Reset the board; the demo runs on UART0 (115200 8N1).

## Notes

- The classic stdio echoes console input for you: `getchar()` calls
  `fputc_cons`, so don't echo the character a second time.
- `__sfr __at 0x81 SERIAL_STATUS;` gives direct `IN A,(0x81)` access; the
  status bits are bit0 = RX ready, bit1 = TX ready.
- The program must fit in page 0 (`0x0000-0x3FFF`); the Pico HEX loader
  replicates that page into all four banks.
