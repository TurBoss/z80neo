// ============================================================================
// HDMI PLL wrapper - uses the exact Gowin rPLL from the working apicula DVI
// example (pll480.v): 27 MHz -> 126 MHz (5x pixel), CLKDIV /5 -> 25.2 MHz.
// ============================================================================

module hdmi_pll (
    input   clk_27m,
    input   rst_n,
    output  serial_clk,   // ~126 MHz (5x pixel)
    output  pix_clk,      // ~25.2 MHz
    output  pll_lock,
    output  hdmi_rst_n
);

    wire lock;

    // Verbatim rPLL from the apicula DVI example (known-good on this board).
    PLL480 u_pll (
        .clkin  (clk_27m),
        .clkout (serial_clk),
        .lock   (lock)
    );

    // Startup timer independent of rst_n: guarantees the serializers/pixel
    // divider are released even if the PLL lock signal is slow to assert.
    reg [21:0] startup_cnt;
    wire       startup_done;

    always @(posedge clk_27m)
        if (!startup_done)
            startup_cnt <= startup_cnt + 22'd1;

    assign startup_done = (startup_cnt > 22'd2_700_000);

    wire combined_rst_n = lock | startup_done;

    CLKDIV #(.DIV_MODE("5")) u_clkdiv (
        .CLKOUT (pix_clk),
        .HCLKIN (serial_clk),
        .RESETN (combined_rst_n)
    );

    assign pll_lock   = lock;
    assign hdmi_rst_n = combined_rst_n;

endmodule
