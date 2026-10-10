// ============================================================================
// nullvdp_ident - wire-identification bitstream
//
// Drives the four NullVDP SPI balls on Dock PMOD02 with distinct slow square
// waves so each ball can be found with a meter/LED/scope, or identified from
// the z80neo by reading MISO and measuring the rate:
//
//   N6  (spi_sck)  ~0.8 Hz   (cnt[24])
//   A11 (spi_mosi) ~1.6 Hz   (cnt[23])
//   B11 (spi_miso) ~3.2 Hz   (cnt[22])   <- the ball MISO should be wired to
//   D11 (spi_cs_n) ~6.4 Hz   (cnt[21])
//
// The LEDs mirror the same four signals.  This is a diagnostic build only:
// no HDMI/PLL/terminal is instantiated.
// ============================================================================
module nullvdp_ident (
    input        clk_27m,
    input        rst_n,
    output       spi_sck,   // N6   ~0.8 Hz
    output       spi_mosi,  // A11  ~1.6 Hz
    output       spi_miso,  // B11  ~3.2 Hz
    output       spi_cs_n,  // D11  ~6.4 Hz
    output [3:0] led
);
    reg [26:0] cnt;
    always @(posedge clk_27m or negedge rst_n) begin
        if (!rst_n) cnt <= 27'd0;
        else        cnt <= cnt + 27'd1;
    end

    assign spi_sck  = cnt[24];
    assign spi_mosi = cnt[23];
    assign spi_miso = cnt[22];
    assign spi_cs_n = cnt[21];
    assign led      = {cnt[24], cnt[23], cnt[22], cnt[21]};
endmodule
