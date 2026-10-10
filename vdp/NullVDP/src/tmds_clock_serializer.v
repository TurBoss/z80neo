// ============================================================================
// TMDS Clock output (Gowin true-LVDS output)
// ============================================================================
//
// The DVI/HDMI TMDS clock is simply the pixel clock on a differential pair
// (matches the apicula DVI example: tmds_clock = clk_pixel).  The data
// channels run at 10x this rate (OSER10 with FCLK = 5x PCLK, DDR).
// ============================================================================

module tmds_clock_serializer (
    input         serial_clk,   // unused
    input         pix_clk,      // ~25 MHz
    input         rst_n,        // unused
    output        tmds_clk_p,
    output        tmds_clk_n
);

    TLVDS_OBUF ob (
        .I  (pix_clk),
        .O  (tmds_clk_p),
        .OB (tmds_clk_n)
    );

endmodule
