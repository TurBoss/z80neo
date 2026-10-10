// ============================================================================
// ULPI register-read probe: read USB3317 reg0 and print "<A> <val> <C0>".
// A fixed 'A' first character confirms the print path is live.
// ============================================================================
module ulpi_host_top (
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

    assign ulpi_rst = 1'b1;
    assign led = {2'b00, ulpi_dir, ulpi_nxt};

    reg        r_start = 1'b0;
    reg        r_wr    = 1'b0;
    reg  [5:0] r_addr  = 6'd0;
    reg  [7:0] r_wdata = 8'd0;
    wire [7:0] r_rdata;
    wire       r_done;
    wire [3:0] dbg_tstate;
    wire [3:0] dbg_rstate;
    reg        dir_low_seen = 1'b0;
    reg [15:0] dirhi = 16'd0;
    reg        rstate_seen = 1'b0;
    reg [3:0]  rstate_first = 4'd0;

    reg        t_req = 1'b0;
    reg  [3:0] t_pid = 4'd0;
    reg  [3:0] t_len = 4'd0;
    reg [63:0] t_data = 64'd0;
    wire       t_busy, t_done;
    wire [7:0]  rx_pid;
    wire [4:0]  rx_len;
    wire [255:0] rx_data;
    wire        rx_valid, rx_error;
    wire [1:0]  line_state;

    ulpi_host u_host (
        .clk(ulclk), .rst_n(ul_rst_n),
        .ulpi_data(ulpi_data), .ulpi_dir(ulpi_dir), .ulpi_nxt(ulpi_nxt),
        .ulpi_stp(ulpi_stp),
        .tx_req(t_req), .tx_pid(t_pid), .tx_len(t_len), .tx_data(t_data),
        .tx_busy(t_busy), .tx_done(t_done),
        .rx_pid(rx_pid), .rx_len(rx_len), .rx_data(rx_data),
        .rx_valid(rx_valid), .rx_error(rx_error), .line_state(line_state),
        .dbg_tstate(dbg_tstate), .dbg_rstate(dbg_rstate),
        .r_start(r_start), .r_wr(r_wr), .r_addr(r_addr), .r_wdata(r_wdata),
        .r_rdata(r_rdata), .r_done(r_done), .r_busy()
    );

    // scalar print registers (avoid memory inference)
    reg [7:0] pr0, pr1, pr2, pr3, pr4, pr5, pr6, pr7, pr8;
    function [7:0] pval; input [3:0] i;
        begin
            case (i)
            4'd0: pval = pr0; 4'd1: pval = pr1; 4'd2: pval = pr2;
            4'd3: pval = pr3; 4'd4: pval = pr4; 4'd5: pval = pr5;
            4'd6: pval = pr6; 4'd7: pval = pr7; default: pval = pr8;
            endcase
        end
    endfunction

    reg [5:0] st = 6'd0;
    reg [19:0] to = 20'd0;

    function [7:0] nib; input [3:0] n; begin nib = (n<10)?(8'h30+n):(8'h37+n); end endfunction

    reg [5:0] byte_idx = 6'd0;
    wire [3:0] pi = byte_idx[5:2];
    reg [7:0] out_byte;
    reg [7:0] pv;
    always @(*) begin
        pv = pval(pi);
        if (byte_idx == 6'd0)            out_byte = 8'h41;         // 'A' marker
        else if (byte_idx[1:0] == 2'd0)  out_byte = nib(pv[7:4]);
        else if (byte_idx[1:0] == 2'd1)  out_byte = nib(pv[3:0]);
        else if (byte_idx[1:0] == 2'd2)  out_byte = 8'h20;
        else                             out_byte = (pi == 4'd8) ? 8'h0D : 8'h20;
    end

    reg  [7:0] tx_data = 8'd0;
    reg        tx_valid = 1'b0;
    wire       tx_ready, tx_busy_u;
    uart_tx #(.CLK_FRE(60), .BAUD_RATE(115200)) u_tx (
        .clk(ulclk), .rst_n(ul_rst_n), .tx_pin(uart_tx),
        .tx_data(tx_data), .tx_data_valid(tx_valid),
        .tx_data_ready(tx_ready), .tx_busy(tx_busy_u)
    );

    always @(posedge ulclk or negedge ul_rst_n) begin
        if (!ul_rst_n) begin
            st <= 6'd0; r_start <= 1'b0; r_wr <= 1'b0; to <= 20'd0;
            tx_valid <= 1'b0; tx_data <= 8'd0; byte_idx <= 6'd0;
            pr0<=8'hA5; pr1<=8'h00; pr2<=8'h00; pr3<=8'h00; pr4<=8'h00;
            pr5<=8'h00; pr6<=8'h00; pr7<=8'h00; pr8<=8'h00;
        end else begin
            if (!ulpi_dir) dir_low_seen <= 1'b1;
            if (ulpi_dir && dirhi != 16'hFFFF) dirhi <= dirhi + 16'd1;
            if (st == 6'd1) to <= to + 20'd1;

            case (st)
            6'd0:  if (por == 8'hFF) begin r_addr <= 6'h00; r_wr <= 1'b0; r_start <= 1'b1; st <= 6'd1; end
            6'd1:  begin
                       pr5 <= {4'b0, dbg_rstate};
                       if (dbg_rstate != 4'd0 && !rstate_seen) begin rstate_seen <= 1'b1; rstate_first <= dbg_rstate; end
                       if (r_done) begin
                           r_start <= 1'b0;
                           pr1 <= r_rdata; pr2 <= 8'hC0;
                           byte_idx <= 6'd0; st <= 6'd2;
                       end else if (to == 20'hFFFFF) begin
                           pr1 <= 8'hEE; pr2 <= 8'hC0;
                           pr3 <= dirhi[15:8];
                           pr4 <= {7'b0, dir_low_seen};
                           pr6 <= {3'b0, rstate_seen, rstate_first};
                           pr7 <= {7'b0, ul_rst_n};
                           pr8 <= {6'b0, r_start, por[0]};
                           byte_idx <= 6'd0; to <= 20'd0; st <= 6'd2;
                       end
                   end
            6'd2:  begin
                       if (tx_valid) begin
                           if (tx_ready) tx_valid <= 1'b0;
                       end else if (byte_idx <= 6'd35) begin
                           tx_data  <= out_byte;
                           tx_valid <= 1'b1;
                           byte_idx <= byte_idx + 6'd1;
                       end else begin
                           byte_idx <= 6'd0;
                           st <= 6'd3;
                       end
                   end
            6'd3:  if (to == 20'hFFFFF) begin to <= 20'd0; st <= 6'd1; end
                   else to <= to + 20'd1;
            default: st <= 6'd0;
            endcase
        end
    end

endmodule
