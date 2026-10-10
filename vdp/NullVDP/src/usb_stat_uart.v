// ============================================================================
// usb_stat_uart - USB host status printer for bring-up.
// Every ~0.25 s prints the N bytes of `v` as hex, v[7:0] first, then CR LF:
//   "03 21 48 ...\r\n"
// ============================================================================
module usb_stat_uart #(
    parameter integer N        = 16,
    parameter [23:0]  TICK_MAX = 24'h66FF2F      // 0.25 s at 27 MHz
) (
    input                clk,
    input                rst_n,
    input      [8*N-1:0] v,
    output reg [7:0]     tx_data,
    output reg           tx_valid,
    input                tx_ready
);
    localparam integer LAST = 3*N + 1;           // 3 chars per byte, then CR, LF
    reg [23:0]    tick;
    reg           run;
    reg [7:0]     ci;
    reg [7:0]     bi;
    reg [1:0]     ph;
    reg [8*N-1:0] snap;
    reg [7:0]     out;

    function [7:0] nib; input [3:0] n;
        begin nib = (n < 10) ? (8'h30 + n) : (8'h37 + n); end
    endfunction

    wire [7:0] bv = snap[bi*8 +: 8];
    always @(*) begin
        if (ci == 3*N)       out = 8'h0D;
        else if (ci == LAST) out = 8'h0A;
        else if (ph == 2'd0) out = nib(bv[7:4]);
        else if (ph == 2'd1) out = nib(bv[3:0]);
        else                 out = 8'h20;
    end

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            tick <= 24'd0; run <= 1'b0; ci <= 8'd0; bi <= 8'd0; ph <= 2'd0; snap <= 0;
            tx_data <= 8'd0; tx_valid <= 1'b0;
        end else if (!run) begin
            if (tx_valid && tx_ready) tx_valid <= 1'b0;
            if (tick == TICK_MAX) begin
                tick <= 24'd0; run <= 1'b1; ci <= 8'd0; bi <= 8'd0; ph <= 2'd0; snap <= v;
            end else tick <= tick + 24'd1;
        end else if (tx_valid) begin
            if (tx_ready) tx_valid <= 1'b0;
        end else begin
            tx_data  <= out;
            tx_valid <= 1'b1;
            if (ci == LAST) run <= 1'b0;
            ci <= ci + 8'd1;
            if (ph == 2'd2) begin ph <= 2'd0; bi <= bi + 8'd1; end else ph <= ph + 2'd1;
        end
    end
endmodule
