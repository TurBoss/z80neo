// ============================================================================
// TMDS Serializer (Gowin OSER10 + true-LVDS output)
// ============================================================================
//
// serial_clk = 5x pixel clock (Gowin OSER10 uses both edges -> 10 bits/pixel).
// The 10-bit TMDS symbol is sent LSB-first (D0 = data[0]).
// TLVDS_OBUF drives the true-LVDS/TMDS differential pair; the cst must set
// IO_TYPE=TMDS33 on both the _p and _n pins.
// ============================================================================

module tmds_serializer (
    input            serial_clk,  // 5x pixel clock (FCLK, ~124.9 MHz)
    input            pix_clk,     // pixel clock     (PCLK, ~25 MHz)
    input            rst_n,
    input      [9:0] data,        // 10-bit TMDS symbol
    output           tmds_p,      // positive TMDS output
    output           tmds_n       // negative TMDS output
);

    wire q;

    OSER10 ser (
        .Q     (q),
        .D0    (data[0]),
        .D1    (data[1]),
        .D2    (data[2]),
        .D3    (data[3]),
        .D4    (data[4]),
        .D5    (data[5]),
        .D6    (data[6]),
        .D7    (data[7]),
        .D8    (data[8]),
        .D9    (data[9]),
        .FCLK  (serial_clk),
        .PCLK  (pix_clk),
        .RESET (~rst_n)
    );

    TLVDS_OBUF ob (
        .I  (q),
        .O  (tmds_p),
        .OB (tmds_n)
    );

endmodule
