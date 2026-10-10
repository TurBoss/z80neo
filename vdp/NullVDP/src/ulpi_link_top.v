// ============================================================================
// ulpi_link read probe: read USB3317 reg0 (expect 0x24) and print
// "<A5> <reg0> <C0>".  Known-good hardware check for the single-module link.
// ============================================================================
module ulpi_link_top (
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

    reg        r_start = 1'b0;
    reg        r_wr    = 1'b0;
    reg  [5:0] r_addr  = 6'd0;
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

    reg [5:0]  st = 6'd0;
    reg [19:0] to = 20'd0;

    function [7:0] nib; input [3:0] n; begin nib = (n<10)?(8'h30+n):(8'h37+n); end endfunction

    reg [5:0] byte_idx = 6'd0;
    wire [3:0] pi = byte_idx[5:2];
    reg [7:0] out_byte;
    reg [7:0] pv;
    always @(*) begin
        pv = pval(pi);
        if (byte_idx == 6'd0)           out_byte = 8'h41;          // 'A'
        else if (byte_idx[1:0] == 2'd0) out_byte = nib(pv[7:4]);
        else if (byte_idx[1:0] == 2'd1) out_byte = nib(pv[3:0]);
        else if (byte_idx[1:0] == 2'd2) out_byte = 8'h20;
        else                            out_byte = (pi == 4'd8) ? 8'h0D : 8'h20;
    end

    reg  [7:0] tx_data = 8'd0;
    reg        tx_valid = 1'b0;
    wire       tx_ready;
    uart_tx #(.CLK_FRE(60), .BAUD_RATE(115200)) u_tx (
        .clk(ulclk), .rst_n(ul_rst_n), .tx_pin(uart_tx),
        .tx_data(tx_data), .tx_data_valid(tx_valid),
        .tx_data_ready(tx_ready), .tx_busy()
    );

    always @(posedge ulclk or negedge ul_rst_n) begin
        if (!ul_rst_n) begin
            st <= 6'd0; r_start <= 1'b0; r_wr <= 1'b0; to <= 20'd0;
            tx_valid <= 1'b0; tx_data <= 8'd0; byte_idx <= 6'd0;
            pr0<=8'hA5; pr1<=8'h00; pr2<=8'h00; pr3<=8'h00; pr4<=8'h00;
            pr5<=8'h00; pr6<=8'h00; pr7<=8'h00; pr8<=8'h00;
        end else begin
            r_start <= 1'b0;
            if (st == 6'd1) to <= to + 20'd1;

            case (st)
            6'd0: if (por == 8'hFF) begin
                      r_addr <= 6'h00; r_wr <= 1'b0; r_start <= 1'b1;
                      to <= 20'd0; st <= 6'd1;
                  end
            6'd1: begin
                      pr5 <= {4'b0, dbg_state};
                      if (r_done) begin
                          r_start <= 1'b0;
                          pr1 <= r_rdata; pr2 <= 8'hC0;
                          byte_idx <= 6'd0; st <= 6'd2;
                      end else if (to == 20'hFFFFF) begin
                          r_start <= 1'b0;
                          pr1 <= 8'hEE;
                          pr6 <= {4'b0, dbg_state};
                          pr7 <= {6'b0, ulpi_dir, ulpi_nxt};
                          byte_idx <= 6'd0; st <= 6'd2;
                      end
                  end
            6'd2: begin
                      if (tx_valid) begin
                          if (tx_ready) tx_valid <= 1'b0;
                      end else if (byte_idx <= 6'd35) begin
                          tx_data  <= out_byte;
                          tx_valid <= 1'b1;
                          byte_idx <= byte_idx + 6'd1;
                      end else begin
                          byte_idx <= 6'd0; to <= 20'd0; st <= 6'd3;
                      end
                  end
            6'd3: if (to == 20'hFFFFF) begin to <= 20'd0; st <= 6'd0; end
                  else to <= to + 20'd1;
            default: st <= 6'd0;
            endcase
        end
    end

endmodule
