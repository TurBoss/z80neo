/*
 * spi_vdp.c - reliable PIO SPI link to the NullVDP
 *
 * The PIO master sends 32-bit frames at about 1 MHz.  Console bytes are
 * sequence-numbered and protected by CRC-8.  The slave acknowledges accepted
 * sequence numbers in the following poll response; an invalid or lost frame
 * is therefore retried without duplicating a byte in the terminal stream.
 */
#include "spi_vdp.h"
#include "memory.h"

#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"

#include "spi_vdp.pio.h"

#define VDP_TX_SIZE       1024
#define VDP_SPI_HZ        2000000u
#define VDP_FRAME_TX      0xA5u
#define VDP_FRAME_POLL    0x5Au
#define VDP_FRAME_SYNC    0x5Bu
#define VDP_RETRY_LIMIT   4u
#define VDP_FLUSH_GUARD   400u

bool vdp_enabled = false;
volatile uint8_t vdp_status = 0;

static uint8_t  vdp_tx[VDP_TX_SIZE];
static uint16_t vdp_tx_head = 0, vdp_tx_tail = 0, vdp_tx_count = 0;
static uint8_t  vdp_next_seq = 0;
static uint8_t  vdp_inflight_seq = 0;
static bool     vdp_inflight = false;
static uint32_t vdp_retry = 0;

static PIO  vdp_pio = pio1;
static uint vdp_sm;
static uint vdp_program_offset;
static pio_sm_config vdp_config;
static bool vdp_pio_ready = false;

volatile bool vdp_spi_ok = false;
volatile uint32_t vdp_id_raw = 0;
volatile uint32_t vdp_last_rx = 0;
volatile uint8_t vdp_miso_pu = 0, vdp_miso_pd = 0;
volatile uint8_t vdp_miso_follows = 0;

static uint8_t crc8_24(uint32_t value)
{
    uint8_t crc = 0;
    for (int index = 2; index >= 0; index--) {
        crc ^= (uint8_t)(value >> (index * 8));
        for (int bit = 0; bit < 8; bit++)
            crc = (crc & 0x80u) ? (uint8_t)((crc << 1) ^ 0x07u) : (uint8_t)(crc << 1);
    }
    return crc;
}

static uint32_t make_frame(uint8_t kind, uint8_t sequence, uint8_t data)
{
    uint32_t frame = ((uint32_t)kind << 24) | ((uint32_t)sequence << 16) |
                     ((uint32_t)data << 8);
    return frame | crc8_24(frame >> 8);
}

static bool response_valid(uint32_t response)
{
    return crc8_24(response >> 8) == (uint8_t)response;
}

static uint32_t vdp_xfer(uint32_t frame)
{
    uint32_t response;

    gpio_put(VDP_SPI_CS_PIN, 0);
    pio_sm_set_enabled(vdp_pio, vdp_sm, true);
    pio_sm_put_blocking(vdp_pio, vdp_sm, frame);
    response = pio_sm_get_blocking(vdp_pio, vdp_sm);
    gpio_put(VDP_SPI_CS_PIN, 1);
    pio_sm_set_enabled(vdp_pio, vdp_sm, false);
    pio_sm_clear_fifos(vdp_pio, vdp_sm);
    pio_sm_init(vdp_pio, vdp_sm, vdp_program_offset, &vdp_config);

    vdp_spi_ok = true;
    vdp_last_rx = response;
    return response;
}

static void vdp_pio_init(void)
{
    pio_sm_config c;
    float div = (float)clock_get_hz(clk_sys) / ((float)VDP_SPI_HZ * 4.0f);

    pio_set_gpio_base(vdp_pio, 16);
    vdp_sm = pio_claim_unused_sm(vdp_pio, true);
    vdp_program_offset = pio_add_program(vdp_pio, &spi_cpha0_program);
    c = spi_cpha0_program_get_default_config(vdp_program_offset);
    sm_config_set_out_pins(&c, VDP_SPI_MOSI_PIN, 1);
    sm_config_set_in_pins(&c, VDP_SPI_MISO_PIN);
    sm_config_set_sideset_pins(&c, VDP_SPI_SCK_PIN);
    sm_config_set_out_shift(&c, false, true, 32);
    sm_config_set_in_shift(&c, false, true, 32);
    sm_config_set_clkdiv(&c, div);

    pio_gpio_init(vdp_pio, VDP_SPI_SCK_PIN);
    pio_gpio_init(vdp_pio, VDP_SPI_MOSI_PIN);
    pio_gpio_init(vdp_pio, VDP_SPI_MISO_PIN);
    uint64_t output_mask = (1ull << VDP_SPI_SCK_PIN) | (1ull << VDP_SPI_MOSI_PIN);
    uint64_t all_mask = output_mask | (1ull << VDP_SPI_MISO_PIN);
    pio_sm_set_pins_with_mask64(vdp_pio, vdp_sm, 0, output_mask);
    pio_sm_set_pindirs_with_mask64(vdp_pio, vdp_sm, output_mask, all_mask);
    pio_sm_init(vdp_pio, vdp_sm, vdp_program_offset, &c);
    vdp_config = c;
    pio_sm_set_enabled(vdp_pio, vdp_sm, false);
    pio_sm_clear_fifos(vdp_pio, vdp_sm);
    vdp_pio_ready = true;
}

// Diagnostic: a deliberately slow poll frame.  The response is in the same
// format as normal traffic, so this remains useful for wiring checks.
uint32_t vdp_probe_slow(void)
{
    return vdp_xfer(make_frame(VDP_FRAME_POLL, 0, 0));
}

void spi_vdp_init(void)
{
    gpio_init(VDP_SPI_CS_PIN);
    gpio_set_dir(VDP_SPI_CS_PIN, GPIO_OUT);
    gpio_put(VDP_SPI_CS_PIN, 1);

    gpio_init(VDP_SPI_MISO_PIN);
    gpio_set_dir(VDP_SPI_MISO_PIN, GPIO_IN);
    gpio_pull_up(VDP_SPI_MISO_PIN);
    busy_wait_us(50);
    vdp_miso_pu = gpio_get(VDP_SPI_MISO_PIN) ? 1u : 0u;
    gpio_pull_down(VDP_SPI_MISO_PIN);
    busy_wait_us(50);
    vdp_miso_pd = gpio_get(VDP_SPI_MISO_PIN) ? 1u : 0u;
    gpio_pull_down(VDP_SPI_MISO_PIN);
    vdp_miso_follows = 0;

    vdp_pio_init();
    // Reset the slave's sequence state independently of FPGA/Pico reset order.
    vdp_xfer(make_frame(VDP_FRAME_SYNC, 0, 0));
    uint32_t response = vdp_xfer(make_frame(VDP_FRAME_POLL, 0, 0));
    vdp_id_raw = response;
    vdp_status = response_valid(response) ? (uint8_t)(response >> 24) : 0;
    vdp_enabled = response_valid(response) &&
                  ((uint8_t)(response >> 8) == VDP_ID_VALUE);
    vdp_next_seq = (uint8_t)(response >> 16) + 1u;
    vdp_inflight = false;

    for (int i = 0; i < 5; i++) {
        gpio_put(LED_PIN, 1); sleep_ms(120);
        gpio_put(LED_PIN, 0); sleep_ms(120);
    }
}

// Wire-identification helper: count rising edges on one pin over a ~2 s window.
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
    if (!vdp_enabled || vdp_tx_count >= VDP_TX_SIZE) return;
    vdp_tx[vdp_tx_head++] = c;
    if (vdp_tx_head >= VDP_TX_SIZE) vdp_tx_head = 0;
    vdp_tx_count++;
}

bool vdp_console_ready(void)
{
    return !vdp_enabled || (vdp_tx_count < VDP_TX_SIZE);
}

static bool vdp_poll(uint8_t *rx_byte)
{
    uint32_t response = vdp_xfer(make_frame(VDP_FRAME_POLL, 0, 0));
    if (!response_valid(response)) return false;

    vdp_status = (uint8_t)(response >> 24);
    *rx_byte = (uint8_t)(response >> 8);
    if (vdp_status & VDP_ST_RX_AVAIL)
        console_inject_rx(*rx_byte);

    if (vdp_inflight && (uint8_t)(response >> 16) == vdp_inflight_seq) {
        vdp_inflight = false;
        vdp_tx_tail = (vdp_tx_tail + 1u) % VDP_TX_SIZE;
        vdp_tx_count--;
        vdp_next_seq++;
        vdp_retry = 0;
    }
    return true;
}

// Re-send the frame still waiting for an acknowledgement.  The FPGA suppresses
// duplicate sequence numbers, so a resend after a lost/corrupted reply (or a
// corrupted write) can never deliver the byte twice.
static void vdp_resend(void)
{
    vdp_xfer(make_frame(VDP_FRAME_TX, vdp_inflight_seq, vdp_tx[vdp_tx_tail]));
}

static bool vdp_try_send(void)
{
    uint8_t rx_byte;
    if (!vdp_enabled || !vdp_pio_ready || !vdp_poll(&rx_byte)) return false;

    if (vdp_inflight) {
        // The acknowledgement has not advanced: the write or its reply was
        // lost.  Retry the write so the link cannot deadlock.
        if (++vdp_retry >= VDP_RETRY_LIMIT) {
            vdp_resend();
            vdp_retry = 0;
        }
        return false;
    }

    if (vdp_tx_count == 0 || !(vdp_status & VDP_ST_TX_READY)) return false;

    vdp_inflight_seq = vdp_next_seq;
    vdp_resend();
    vdp_inflight = true;
    vdp_retry = 0;
    return true;
}

void vdp_console_flush(void)
{
    if (!vdp_enabled) return;
    // Bounded so a persistently broken link degrades to dropped bytes instead
    // of stalling the boot (the boot logo calls this once per byte).
    uint32_t guard = 0;
    while (vdp_tx_count > 0 && guard++ < VDP_FLUSH_GUARD)
        vdp_try_send();
    guard = 0;
    while (vdp_inflight && guard++ < VDP_FLUSH_GUARD)
        vdp_try_send();
    // If the link is still stuck, abandon the frame so later output can make
    // progress instead of blocking every future byte.
    if (vdp_inflight) {
        vdp_inflight = false;
        vdp_tx_tail = (vdp_tx_tail + 1u) % VDP_TX_SIZE;
        if (vdp_tx_count > 0) vdp_tx_count--;
        vdp_next_seq++;
        vdp_retry = 0;
    }
}

void spi_vdp_poll(void)
{
    if (!vdp_enabled) return;
    vdp_try_send();
}
