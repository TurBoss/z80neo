// ============================================================================
// overlay_ram.v - menu overlay cells (dual-clock BRAM, like char_buffer)
// ============================================================================
// 520 cells x {attr[15:0], char[7:0]}, written by the menu (system clock) and
// read by the text controller (pixel clock) to paint the menu box.

module overlay_ram (
    input             clk_rd,
    input      [9:0]  rd_addr,
    output reg [7:0]  rd_char,
    output reg [15:0] rd_attr,

    input             clk_wr,
    input             wr_en,
    input      [9:0]  wr_addr,
    input      [7:0]  wr_char,
    input      [15:0] wr_attr
);

    reg [23:0] mem [0:519];

    always @(posedge clk_rd) begin
        {rd_attr, rd_char} <= mem[rd_addr];
    end

    always @(posedge clk_wr) begin
        if (wr_en)
            mem[wr_addr] <= {wr_attr, wr_char};
    end

endmodule
