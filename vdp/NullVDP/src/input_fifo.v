// ============================================================================
// Input FIFO - decouples the byte source from the terminal handler
// ============================================================================
//
// The terminal handler blocks for up to a full-screen fill (~2400 cycles)
// while erasing/scrolling.  Without a FIFO, bytes arriving during that time
// would be dropped.  Fall-through (asynchronous read) so the handler can
// consume a byte in the same cycle it becomes available.

module input_fifo #(
    parameter DEPTH = 64,
    parameter AW    = 6
) (
    input            clk,
    input            rst_n,
    input            push,
    input      [7:0] din,
    input            pop,
    output     [7:0] dout,
    output           empty,
    output           full
);

    reg [7:0] mem [0:DEPTH-1];
    reg [AW:0] wptr, rptr;      // one extra bit to distinguish full/empty

    wire [AW:0] wnext = wptr + 1'b1;
    wire [AW:0] rnext = rptr + 1'b1;

    assign dout  = mem[rptr[AW-1:0]];
    assign empty = (wptr == rptr);
    assign full  = (wnext == rptr);

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            wptr <= {AW+1{1'b0}};
            rptr <= {AW+1{1'b0}};
        end else begin
            if (push && !full)
                mem[wptr[AW-1:0]] <= din;
            wptr <= push && !full ? wnext : wptr;
            rptr <= pop  && !empty ? rnext : rptr;
        end
    end

endmodule
