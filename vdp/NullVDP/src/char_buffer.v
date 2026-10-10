// ============================================================================
// Character Buffer - Dual-port BRAM for text framebuffer
// 80 columns x 30 rows = 2400 bytes
// ============================================================================
//
// Port A: Read port (pixel clock domain)  - used by text_controller
// Port B: Write port (system clock domain) - used by terminal_handler
//
// True dual-port BRAM with separate clocks for each port.

module char_buffer (
    // Port A - Read (pix_clk domain)
    input            clk_rd,      // 25 MHz pixel clock
    input      [11:0] rd_addr,    // 0..2399
    output reg [7:0]  rd_data,    // registered output

    // Port B - Write (system clock domain)
    input            clk_wr,      // 27 MHz system clock
    input            wr_en,
    input      [11:0] wr_addr,    // 0..2399
    input      [7:0]  wr_data
);

    // 2400 x 8-bit BRAM
    reg [7:0] mem [0:2399];

    // Port A: Read (synchronous, 1 cycle latency)
    always @(posedge clk_rd) begin
        rd_data <= mem[rd_addr];
    end

    // Port B: Write (synchronous)
    always @(posedge clk_wr) begin
        if (wr_en)
            mem[wr_addr] <= wr_data;
    end

endmodule
