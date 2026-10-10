# NullVDP SPI link protocol

The z80neo (RP2350) drives the NullVDP (Tang Primer 20K FPGA terminal) over a
short **SPI** link.  The Pico is the **master**; the NullVDP is the **slave**.
This replaces the bring-up UART as the display transport while keeping the same
console byte stream (`doc/vdp_protocol.md`) — text, CSI sequences and the
`ESC g` graphics protocol all travel unchanged.

## Physical

| Signal | z80neo (master) | NullVDP (slave) | Notes |
|--------|-----------------|-----------------|-------|
| SCK    | GPIO36          | N6  (IOR51A)    | Dock PMOD03, bank 3, 3.3 V |
| MOSI   | GPIO35          | A11 (IOL15A)    | Dock PMOD03, bank 7, 3.3 V |
| MISO   | GPIO34          | B11 (IOL13A)    | Dock PMOD03, bank 7, 3.3 V |
| CS_N   | GPIO37          | D11 (IOL22A)    | Dock PMOD03, bank 7, 3.3 V, active low |
| GND    | GND             | GND             | common ground required |

**Do not use the Dock's bank-0/1 pins for the link** (e.g. PMOD02's D14 =
`1/IOT44A`).  HDMI drives the TMDS over true-LVDS, which forces IO banks 0/1
into LVDS mode, so those balls cannot do single-ended LVCMOS while HDMI is
active — the SPI clock silently dies.  PMOD03 (banks 3/7) is safe.

Wiring is 3.3 V direct (no level shifting).  Keep the wires short.  The firmware
master is a plain C bit-bang at ~250 kHz (`spi_vdp.c`, `VDP_SPI_DELAY_US = 2`);
the slave oversamples SCK in its 27 MHz domain, so keep SCK ≲ 6 MHz.
(`src/spi_vdp.pio` is a PIO variant that is generated but not yet wired in.)

## Framing

SPI **mode 0** (CPOL = 0, CPHA = 0), **MSB-first**, active-low CS.  Each
transaction is **16 bits**, held under one CS pulse:

```
master -> slave :  CMD[7:0]   DATA[7:0]
slave  -> master:  STATUS[7:0] RDATA[7:0]
```

`CMD = (addr[6:0] << 1) | rw` where `rw = 1` is a write and `rw = 0` a read.
`STATUS` is returned in the **first** byte of *every* transaction, so the master
learns TX_READY / RX_AVAIL from the previous transfer (one transfer of
pipeline).  `RDATA` is selected by the current `CMD` and is valid in the second
byte.  A write ignores `RDATA`; a read ignores `DATA`.

## Registers

| addr | name | R/W | value |
|------|------|-----|-------|
| 0x00 | STATUS  | R | bit0 TX_READY, bit1 RX_AVAIL, bit2 RX_OVF, bit7 = 1 |
| 0x01 | TX_DATA | W | console byte to display |
| 0x02 | RX_DATA | R | keyboard byte for the host |
| 0x03 | CTRL    | W | write 1 clears RX_OVF |
| 0x04 | ID      | R | `0x4E` ('N') |

- **TX_READY** — the slave's 8-byte console FIFO has space.  A write with
  TX_READY clear is dropped.
- **RX_AVAIL** — a keyboard byte is waiting; read it with `RX_DATA`.  Reading
  `RX_DATA` pops the FIFO only when RX_AVAIL was set.
- **RX_OVF** — a keyboard byte was dropped because the FIFO was full.

## Master behaviour (z80neo firmware, `firmware/z80neo/src/spi_vdp.c`)

- `spi_vdp_init()` probes `ID`; the link is enabled only if it reads `0x4E`
  (so an unwired link is inert and cannot inject garbage).
- The main loop mirrors every byte drained from the console TX ring
  (`tx_buffer`, fed by port `0x80` and `console_putc`) to the VDP.
- Each `spi_vdp_poll()` push one console byte when TX_READY, otherwise reads
  `RX_DATA`; bytes the slave reports are injected into the console RX ring so
  the Z80 sees them on port `0x80` as ordinary console input.
- Z80 ports: `0x94` write = VDP console byte, `0x95` read = STATUS.

## Slave behaviour (NullVDP `src/spi_slave.v`)

- SPI bytes feed the terminal as the highest-priority console source.
- PS/2 keyboard bytes are pushed to the RX FIFO for the host.  Local echo is
  controlled by the menu (AUTO/ON/OFF): AUTO suppresses the local PS/2 echo when
  the host is driving the display (recent SPI traffic) so characters are not
  doubled (the host echoes them back), ON forces local echo even while the host
  polls, OFF never echoes locally.
- The TX/RX FIFOs are 8 bytes deep.

## Sim

`vdp/NullVDP/sim/tb_spi.v` models the master and checks register access,
TX handoff/back-pressure and RX reads: `make sim-spi`.
