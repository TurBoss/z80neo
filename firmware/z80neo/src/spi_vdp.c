/*
 * spi_vdp.c - NullVDP SPI link (z80neo master side)
 *
 * Software bit-banged SPI master, mode 0 (CPOL=0, CPHA=0), MSB-first, 16-bit
 * frames.  SCK/MOSI are plain GPIO outputs, MISO a plain input and CS a GPIO
 * toggled around each frame.  See include/spi_vdp.h and the NullVDP
 * spi_slave.v for the register protocol.
 */
#include "spi_vdp.h"
#include "memory.h"

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"

#define VDP_TX_SIZE       1024
#define VDP_SPI_DELAY_US  2      // ~250 kHz SCK

bool vdp_enabled = false;
volatile uint8_t vdp_status = 0;

static uint8_t  vdp_tx[VDP_TX_SIZE];
static uint16_t vdp_tx_head = 0, vdp_tx_tail = 0, vdp_tx_count = 0;

volatile bool vdp_spi_ok = false;
volatile uint32_t vdp_id_raw = 0;
volatile uint32_t vdp_last_rx = 0;
volatile uint8_t vdp_miso_pu = 0, vdp_miso_pd = 0;
volatile uint8_t vdp_miso_follows = 0;

// One 16-bit frame: send {cmd,data}, return {status,rdata}.  MISO is sampled on
// the SCK rising edge; MOSI changes while SCK is low (CPHA=0).
static inline uint16_t vdp_xfer_d(unsigned dly, uint8_t cmd, uint8_t data)
{
    uint16_t frame = ((uint16_t)cmd << 8) | data;
    uint16_t r = 0;

    gpio_put(VDP_SPI_CS_PIN, 0);
    busy_wait_us(dly);
    for (int i = 15; i >= 0; i--) {
        gpio_put(VDP_SPI_MOSI_PIN, (frame >> i) & 1u);
        busy_wait_us(dly);
        gpio_put(VDP_SPI_SCK_PIN, 1);
        busy_wait_us(dly);
        r = (uint16_t)((r << 1) | (gpio_get(VDP_SPI_MISO_PIN) ? 1u : 0u));
        gpio_put(VDP_SPI_SCK_PIN, 0);
        busy_wait_us(dly);
    }
    gpio_put(VDP_SPI_CS_PIN, 1);
    busy_wait_us(dly);

    vdp_spi_ok = true;
    vdp_last_rx = r;
    return r;
}

static inline uint16_t vdp_xfer(uint8_t cmd, uint8_t data)
{
    return vdp_xfer_d(VDP_SPI_DELAY_US, cmd, data);
}

// Diagnostic: an ID read at a very slow SCK (~2.5 kHz) to separate a wiring/
// protocol fault from an edge-rate/signal-integrity one.
uint16_t vdp_probe_slow(void)
{
    return vdp_xfer_d(200, (uint8_t)((VDP_REG_ID << 1) | 0), 0);
}

static inline uint8_t vdp_cmd(uint8_t addr, uint8_t rw)
{
    return (uint8_t)((addr << 1) | (rw & 1));
}

void spi_vdp_init(void)
{
    gpio_init(VDP_SPI_SCK_PIN);
    gpio_set_dir(VDP_SPI_SCK_PIN, GPIO_OUT);
    gpio_put(VDP_SPI_SCK_PIN, 0);
    gpio_init(VDP_SPI_MOSI_PIN);
    gpio_set_dir(VDP_SPI_MOSI_PIN, GPIO_OUT);
    gpio_put(VDP_SPI_MOSI_PIN, 0);
    gpio_init(VDP_SPI_MISO_PIN);
    gpio_set_dir(VDP_SPI_MISO_PIN, GPIO_IN);
    // Wiring probe: if pu=1,pd=0 the line floats (nothing driving it); if
    // pu=0,pd=0 the slave holds it low; pu=1,pd=1 it is stuck high.
    gpio_pull_up(VDP_SPI_MISO_PIN);
    busy_wait_us(50);
    vdp_miso_pu = gpio_get(VDP_SPI_MISO_PIN) ? 1u : 0u;
    gpio_pull_down(VDP_SPI_MISO_PIN);
    busy_wait_us(50);
    vdp_miso_pd = gpio_get(VDP_SPI_MISO_PIN) ? 1u : 0u;
    gpio_pull_down(VDP_SPI_MISO_PIN);   // unwired link reads 0
    gpio_init(VDP_SPI_CS_PIN);
    gpio_set_dir(VDP_SPI_CS_PIN, GPIO_OUT);
    gpio_put(VDP_SPI_CS_PIN, 1);

    // (Follow / wire-short test removed: driving our own CS/SCK/MOSI here left
    // the slave mid-frame and corrupted the first ID probe, so the link gate
    // never opened.  The ident sweep covers wiring instead.)
    vdp_miso_follows = 0;

    // Probe the ID register; the software master always clocks a frame.  The
    // link is enabled only if the slave answers with its ID, so an unwired link
    // (MISO pulled down -> reads 0) stays inert and cannot inject garbage.
    uint16_t r = vdp_xfer(vdp_cmd(VDP_REG_ID, 0), 0);
    vdp_id_raw = r;
    vdp_status = (uint8_t)(r >> 8);
    vdp_enabled = ((uint8_t)(r & 0xFF) == VDP_ID_VALUE);

    // Boot diagnostic on the Pico LED: 5 quick blinks.
    for (int i = 0; i < 5; i++) {
        gpio_put(LED_PIN, 1); sleep_ms(120);
        gpio_put(LED_PIN, 0); sleep_ms(120);
    }
}

// Wire-identification helper: count rising edges on one pin over a ~2 s window.
// With the nullvdp_ident bitstream loaded, each SPI ball carries a distinct
// square wave, so the edge count identifies the ball a Pico pin is wired to:
// ~1 N6/SCK, ~3 A11/MOSI, ~6 B11/MISO, ~13 D11/CS (edges over 2 s).
uint32_t vdp_probe_pin_edges(uint32_t pin)
{
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_down(pin);
    uint32_t edges = 0;
    bool last = gpio_get(pin);
    uint64_t t0 = time_us_64();
    while ((time_us_64() - t0) < 2000000ull) {
        bool now = gpio_get(pin);
        if (now && !last) edges++;
        last = now;
    }
    return edges;
}

void vdp_console_tx(uint8_t c)
{
    if (!vdp_enabled) return;
    if (vdp_tx_count >= VDP_TX_SIZE) return;   // safety valve
    vdp_tx[vdp_tx_head++] = c;
    if (vdp_tx_head >= VDP_TX_SIZE) vdp_tx_head = 0;
    vdp_tx_count++;
}

// Send every queued byte now, honouring the slave's TX_READY so nothing drops.
// Blocks; intended for the boot log (the main loop uses spi_vdp_poll instead).
void vdp_console_flush(void)
{
    if (!vdp_enabled) return;
    while (vdp_tx_count > 0) {
        if (vdp_status & VDP_ST_TX_READY) {
            uint8_t b = vdp_tx[vdp_tx_tail];
            uint16_t r = vdp_xfer(vdp_cmd(VDP_REG_TX, 1), b);
            vdp_status = (uint8_t)(r >> 8);
            vdp_tx_tail++;
            if (vdp_tx_tail >= VDP_TX_SIZE) vdp_tx_tail = 0;
            vdp_tx_count--;
        } else {
            // FIFO full: a read refreshes STATUS and gives the slave time.
            uint16_t r = vdp_xfer(vdp_cmd(VDP_REG_RX, 0), 0);
            vdp_status = (uint8_t)(r >> 8);
        }
    }
}

void spi_vdp_poll(void)
{
    if (!vdp_enabled) return;

    if (vdp_tx_count > 0) {
        uint8_t b = vdp_tx[vdp_tx_tail];
        uint16_t r = vdp_xfer(vdp_cmd(VDP_REG_TX, 1), b);
        vdp_status = (uint8_t)(r >> 8);
        vdp_tx_tail++;
        if (vdp_tx_tail >= VDP_TX_SIZE) vdp_tx_tail = 0;
        vdp_tx_count--;
    } else {
        // Nothing to send: fetch a keyboard byte (also refreshes status).
        uint16_t r = vdp_xfer(vdp_cmd(VDP_REG_RX, 0), 0);
        vdp_status = (uint8_t)(r >> 8);
        if (vdp_status & VDP_ST_RX_AVAIL) {
            console_inject_rx((uint8_t)(r & 0xFF));
        }
    }
}
