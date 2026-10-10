# NullVDP SPI link protocol

The z80neo RP2350 drives the NullVDP Tang Primer 20K terminal over a short
3.3 V SPI link. The Pico is the master; the NullVDP is the slave. The link
uses PIO and acknowledged 32-bit frames, while the payload remains the same
console byte stream used by UART and the `ESC g` graphics protocol.

## Physical

| Signal | z80neo | NullVDP | Notes |
|--------|--------|---------|-------|
| SCK | GPIO36 | N6 | Dock PMOD03 |
| MOSI | GPIO35 | A11 | Dock PMOD03 |
| MISO | GPIO34 | B11 | Dock PMOD03 |
| CS_N | GPIO37 | D11 | Dock PMOD03, active low |
| GND | GND | GND | common ground required |

Do not use Dock bank-0/1 pins: HDMI forces those FPGA banks into LVDS mode.
The PIO master currently runs at approximately 2 MHz; the FPGA synchronizes
the external SCK into its 27 MHz clock domain.

## Framing

SPI mode 0, MSB-first, active-low CS. Each transaction is 32 bits:

```text
master -> slave: KIND SEQUENCE DATA CRC8
slave  -> master: STATUS ACK RXDATA CRC8
```

CRC-8 uses polynomial `0x07` over the first three bytes, with an initial value
of zero.

`KIND=0xA5` carries one console byte. `KIND=0x5A` is a poll frame and carries
no data. `KIND=0x5B` synchronizes the link, clearing the slave TX sequence and
FIFO; the master sends it at startup before polling. The first synchronized
poll response places the ID `0x4E` (`N`) in `RXDATA`.
Later poll responses place a keyboard byte there when `RX_AVAIL` is set.

`ACK` is the last accepted console sequence number. A frame with a bad CRC or
unexpected sequence is ignored. If the master repeats the last frame after a
lost response, the slave acknowledges the duplicate without delivering the
byte twice. This prevents one electrical error from desynchronizing the
terminal parser.

## Status

The response `STATUS` byte contains:

- bit 0: TX FIFO has space
- bit 1: keyboard RX byte available
- bit 2: keyboard RX overflow occurred
- bit 7: NullVDP present

The console TX and keyboard RX FIFOs are eight bytes deep. The master retries
TX frames while the slave reports a full TX FIFO.

## Firmware behaviour

`spi_vdp_init()` configures the PIO master and probes the ID. The main loop
mirrors every byte drained from the console TX ring, fed by Z80 port `0x80`
and firmware `console_putc()`, into acknowledged frames. Z80 port `0x94`
remains a direct console-byte enqueue path, and port `0x95` reports the latest
link status.

## Simulation

`vdp/NullVDP/sim/tb_spi.v` checks ID, framed TX delivery, duplicate suppression,
keyboard RX, and status handling. `tb_spi_gfx.v` replays a captured console
stream through the framed SPI slave, terminal parser, and graphics engine.
