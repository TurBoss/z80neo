/*
 * spi_vdp.h - NullVDP SPI link (z80neo master side)
 *
 * The Pico is the SPI master; the NullVDP (FPGA) is the slave.  Console output
 * written to the TX ring (mirrored from port 0x80 / console_putc) is streamed
 * to the VDP, and keyboard bytes the VDP reports are injected into the UART RX
 * ring so the Z80 sees them on port 0x80 as normal console input.
 */
#ifndef SPI_VDP_H
#define SPI_VDP_H

#include <stdint.h>
#include <stdbool.h>

/* Slave register addresses (see the NullVDP spi_slave.v). */
#define VDP_REG_STATUS 0x00
#define VDP_REG_TX     0x01
#define VDP_REG_RX     0x02
#define VDP_REG_CTRL   0x03
#define VDP_REG_ID     0x04

#define VDP_ST_TX_READY 0x01
#define VDP_ST_RX_AVAIL 0x02
#define VDP_ST_RX_OVF   0x04

/* ID register value ('N'), used to detect a wired slave at init. */
#define VDP_ID_VALUE    0x4E

/* Set by the INI (VDP=1) to enable the link. */
extern bool vdp_enabled;

void spi_vdp_init(void);

/* Queue a console byte for the VDP (drained by spi_vdp_poll). */
void vdp_console_tx(uint8_t c);

/* Block until every queued VDP console byte has been sent (flow-controlled).
   Used to mirror the boot log synchronously. */
void vdp_console_flush(void);

/* One step of the link: push a TX byte or pull a key.  Call from the main loop. */
void spi_vdp_poll(void);

/* Wire-ID helper (nullvdp_ident bitstream): rising edges on one pin over ~2 s. */
uint32_t vdp_probe_pin_edges(uint32_t pin);

/* Diagnostic: one very slow (~2.5 kHz) ID read. */
uint16_t vdp_probe_slow(void);

/* Last status byte read from the slave (bit0/1/2 above). */
extern volatile uint8_t vdp_status;

/* MISO level with the Pico pull-up (pu) / pull-down (pd) applied, sampled at
   init before the first frame: pu=1,pd=0 means the line floats (not driven);
   pu=0,pd=0 means it is driven low (slave present); pu=1,pd=1 driven high. */
extern volatile uint8_t vdp_miso_pu, vdp_miso_pd;

/* If the MISO wire is shorted/swapped onto one of our own SPI outputs this is
   the victim: 1 = CS, 2 = SCK, 3 = MOSI, 0 = none (MISO is a real input). */
extern volatile uint8_t vdp_miso_follows;

/* Raw last-frame readbacks (debug). */
extern volatile uint32_t vdp_id_raw;
extern volatile uint32_t vdp_last_rx;

#endif
