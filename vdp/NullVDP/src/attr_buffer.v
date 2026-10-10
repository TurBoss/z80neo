// ============================================================================
// Attribute Buffer - Dual-port BRAM for per-cell colour/attribute
// 80 columns x 30 rows = 2400 x 16 bits (same addressing as char_buffer)
// ============================================================================
//
// Attribute word layout (see terminal_handler.v):
//   [3:0]  foreground palette index
//   [7:4]  background palette index
//   [8]    underline
//   [9]    blink
//   [10]   inverse
//
// Port A: Read (pix_clk), Port B: Write (system clk).

module attr_buffer (
    input             clk_rd,
    input      [11:0] rd_addr,
    output reg [15:0] rd_data,

    input             clk_wr,
    input             wr_en,
    input      [11:0] wr_addr,
    input      [15:0] wr_data,
);

    reg [15:0] mem [0:2399];

    always @(posedge clk_rd) begin
        rd_data <= mem[rd_addr];
    end

    always @(posedge clk_wr) begin
        if (wr_en)
            mem[wr_addr] <= wr_data;
    end

endmodule
