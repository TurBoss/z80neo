# z80neo UART1 terminal bridge

A small bare-metal Z80 program (`+z80neo` target) that bridges the board's
console to the second UART the firmware exposes.  It lets you talk to a device
wired to **UART1 (GPIO36 = TX, GPIO37 = RX)** from the normal console terminal
(the VDP or the PC `sdl_client`).

It is a raw bidirectional bridge:

```
console RX (0x80 read)  ->  UART1 TX (0x90 write)
UART1 RX  (0x90 read)   ->  console TX (0x80 write)
```

## Ports

| Port | Direction | Meaning |
|------|-----------|---------|
| `0x80` | out | console data |
| `0x80` | in  | console data (RX) |
| `0x81` | in  | console status: bit0 = RX ready, bit1 = TX ready |
| `0x90` | out | UART1 data |
| `0x90` | in  | UART1 data (RX) |
| `0x91` | in  | UART1 status: bit0 = RX ready, bit1 = TX ready |

## Build

```sh
make                 # -> build/uart1term.ihx  (Intel HEX)
make bin             # -> build/uart1term      (raw binary)
make Z88DK=/path/to/z88dk
```

`make` uses `../../z88dk` by default.

## Deploy

1. Copy `build/uart1term.ihx` to the SD-card root.
2. Point the first program in `Z80NEO.INI` at it (`BANK_PROG[0]`):

   ```
   cpmz80neo.hex   ->   uart1term.ihx
   ```
3. Reset the board.  The banner appears on the console (UART0, 115200 8N1);
   type to send to UART1, and UART1 data is echoed back to the console.

## Notes

- Both UARTs are **115200 8N1** by default.
- Run the console terminal with **local echo OFF** (the VDP and `sdl_client`
  already are) — otherwise a typed character can echo back and forth.
- The RP2350 pads are 3.3 V.  A 5 V TTL or RS-232 device on UART1 needs level
  shifting (RS-232 also needs a transceiver).
- There is no exit: the program is the whole machine, so reset the board to
  leave the bridge.
