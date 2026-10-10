// ============================================================================
// GFX Buffer - 3 bitplane bitmap layer for the ESC g graphics protocol
//
// 640x256 = 163840 pixels, one bit per plane.  Three planes give an 8-colour
// (3bpp) palette index per pixel; the index selects a palette entry in the
// renderer (see text_controller.v).  Stored as three 1-bit-wide memories so a
// pixel write is a plain word write (no read-modify-write of neighbours) and
// each plane costs the minimum 10 BSRAM blocks (163840 = 10 x 16384).
//
// Port A: read  (pix_clk domain, text_controller),  3 bits out
// Port B: write (system clock domain, gfx_engine),   3 bits in
//
// Address = y * 640 + x  (0 .. 163839).
// ============================================================================

module gfx_buffer #(
    parameter DEPTH = 163840
) (
    input             clk_rd,      // pixel clock
    input      [17:0] rd_addr,
    output reg [2:0]  rd_data,     // {plane2, plane1, plane0}

    input             clk_wr,      // system clock
    input      [2:0]  we,
    input      [17:0] wr_addr,
    input      [2:0]  wr_data
);

    reg p0 [0:DEPTH-1];
    reg p1 [0:DEPTH-1];
    reg p2 [0:DEPTH-1];

    always @(posedge clk_rd) begin
        rd_data <= {p2[rd_addr], p1[rd_addr], p0[rd_addr]};
    end

    always @(posedge clk_wr) begin
        if (we[0]) p0[wr_addr] <= wr_data[0];
        if (we[1]) p1[wr_addr] <= wr_data[1];
        if (we[2]) p2[wr_addr] <= wr_data[2];
    end

endmodule
