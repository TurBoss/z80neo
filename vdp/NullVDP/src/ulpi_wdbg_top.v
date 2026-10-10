// ============================================================================
// Minimal ULPI register-write probe with a periodic state dump.
//   por -> write FC=0x4C -> write OTG=0x26 -> idle
// every ~0.2 s prints 6 hex bytes:
//   <st> <link_state> <dir> <nxt> <r_done> <FC readback>
// ============================================================================
module ulpi_wdbg_top (
    input        clk_27m,
    input        rst_n,
    input        ulpi_clk,
    input        ulpi_dir,
    input        ulpi_nxt,
    inout  [7:0] ulpi_data,
    output       ulpi_stp,
    output       ulpi_rst,
    output       uart_tx,
    output [3:0] led
);
    wire ulclk = ~ulpi_clk;

    reg [7:0] por = 8'd0;
    always @(posedge ulclk) if (por != 8'hFF) por <= por + 8'd1;
    wire ul_rst_n = rst_n & (por == 8'hFF);

    reg [15:0] prst = 16'd0;
    always @(posedge clk_27m) if (prst != 16'hFFFF) prst <= prst + 16'd1;
    assign ulpi_rst = rst_n & (prst == 16'hFFFF);
    assign led = {2'b00, ulpi_dir, ulpi_nxt};

    reg        r_start = 1'b0, r_wr = 1'b0;
    reg  [5:0] r_addr = 6'd0;
    reg  [7:0] r_wdata = 8'd0;
    wire [7:0] r_rdata;
    wire       r_done;
    wire [3:0] dbg_state;

    ulpi_link u_link (
        .clk(ulclk), .rst_n(ul_rst_n),
        .ulpi_data(ulpi_data), .ulpi_dir(ulpi_dir), .ulpi_nxt(ulpi_nxt),
        .ulpi_stp(ulpi_stp),
        .r_req(r_start), .r_wr(r_wr), .r_addr(r_addr), .r_wdata(r_wdata),
        .r_rdata(r_rdata), .r_done(r_done), .r_busy(),
        .tx_req(1'b0), .tx_pid(4'd0), .tx_len(4'd0), .tx_data(96'd0),
        .tx_busy(), .tx_done(),
        .rx_pid(), .rx_len(), .rx_data(), .rx_valid(), .rx_error(),
        .line_state(), .dbg_state(dbg_state)
    );

    reg [7:0] pr0, pr1, pr2, pr3, pr4, pr5;
    reg [7:0] pv;
    function [7:0] pval; input [3:0] i;
        begin
            case (i)
            4'd0: pval=pr0; 4'd1: pval=pr1; 4'd2: pval=pr2;
            4'd3: pval=pr3; 4'd4: pval=pr4; default: pval=pr5;
            endcase
        end
    endfunction
    function [7:0] nib; input [3:0] n; begin nib=(n<10)?(8'h30+n):(8'h37+n); end endfunction

    reg [3:0] byte_idx = 4'd0;
    reg [7:0] out_byte;
    always @(*) begin
        pv = pval(byte_idx[2:1]);
        case (byte_idx[0])
        1'b0: out_byte = nib(pv[7:4]);
        1'b1: out_byte = (byte_idx == 4'd11) ? 8'h0D : nib(pv[3:0]);
        endcase
    end

    reg  [7:0] u_txdata = 8'd0;
    reg        u_txval = 1'b0;
    wire       u_txrdy;
    uart_tx #(.CLK_FRE(60), .BAUD_RATE(115200)) u_tx (
        .clk(ulclk), .rst_n(ul_rst_n), .tx_pin(uart_tx),
        .tx_data(u_txdata), .tx_data_valid(u_txval),
        .tx_data_ready(u_txrdy), .tx_busy()
    );

    localparam [2:0] W1=3'd0, W2=3'd1, IDLE=3'd2, PRINT=3'd3;
    reg [2:0]  st = 3'd0;
    reg [22:0] pt = 23'd0;

    always @(posedge ulclk or negedge ul_rst_n) begin
        if (!ul_rst_n) begin
            st <= W1; r_start <= 1'b0; r_wr <= 1'b0;
            u_txval <= 1'b0; u_txdata <= 8'd0; byte_idx <= 4'd0; pt <= 23'd0;
            pr0<=8'h00; pr1<=8'h00; pr2<=8'h00; pr3<=8'h00; pr4<=8'h00; pr5<=8'h00;
        end else begin
            r_start <= 1'b0;

            case (st)
            W1: if (por == 8'hFF) begin r_addr<=6'h04; r_wdata<=8'h4C; r_wr<=1'b1; r_start<=1'b1; st<=W2; end
            W2: if (r_done) begin r_addr<=6'h06; r_wdata<=8'h26; r_wr<=1'b1; r_start<=1'b1; st<=IDLE; end
            IDLE: begin
                pt <= pt + 23'd1;
                if (pt == 23'h3FFFFF) begin
                    pt <= 23'd0;
                    pr0 <= {5'b0, st};
                    pr1 <= {4'b0, dbg_state};
                    pr2 <= {6'b0, ulpi_dir, ulpi_nxt};
                    pr3 <= {7'b0, r_done};
                    pr4 <= {6'b0, pr4[1:0]};
                    pr5 <= r_rdata;
                    byte_idx <= 4'd0; st <= PRINT;
                end
            end
            PRINT: begin
                if (u_txval) begin
                    if (u_txrdy) u_txval <= 1'b0;
                end else if (byte_idx <= 4'd11) begin
                    u_txdata <= out_byte; u_txval <= 1'b1;
                    byte_idx <= byte_idx + 4'd1;
                end else begin
                    byte_idx <= 4'd0; st <= IDLE;
                end
            end
            default: st <= W1;
            endcase
        end
    end
endmodule
