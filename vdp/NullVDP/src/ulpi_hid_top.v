// ============================================================================
// ULPI host bring-up (one-shot):
//   init PHY -> bus reset -> GET_DESCRIPTOR(device,8) at addr 0 -> print
//   "<linestate> <rx_pid> <rx_len> <FC> <OTG> <b0> <b1> <b2> <b3>"
// A working device yields rx_pid=C3 and bytes 12 01 00 02 ...
// ============================================================================
module ulpi_hid_top (
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

    reg        t_req  = 1'b0;
    reg  [3:0] t_pid  = 4'd0;
    reg  [3:0] t_len  = 4'd0;
    reg [63:0] t_data = 64'd0;
    wire       t_done;
    wire [7:0]   rx_pid;
    wire [4:0]   rx_len;
    wire [255:0] rx_data;
    wire         rx_valid;
    wire [1:0]   line_state;
    wire [3:0]   dbg_state;

    ulpi_link u_link (
        .clk(ulclk), .rst_n(ul_rst_n),
        .ulpi_data(ulpi_data), .ulpi_dir(ulpi_dir), .ulpi_nxt(ulpi_nxt),
        .ulpi_stp(ulpi_stp),
        .r_req(r_start), .r_wr(r_wr), .r_addr(r_addr), .r_wdata(r_wdata),
        .r_rdata(r_rdata), .r_done(r_done), .r_busy(),
        .tx_req(t_req), .tx_pid(t_pid), .tx_len(t_len), .tx_data(t_data),
        .tx_busy(), .tx_done(t_done),
        .rx_pid(rx_pid), .rx_len(rx_len), .rx_data(rx_data),
        .rx_valid(rx_valid), .rx_error(), .line_state(line_state),
        .dbg_state(dbg_state)
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

    localparam [5:0]
        S_INIT1=6'd0, S_INIT2=6'd1, S_SE0=6'd2, S_SE0W=6'd3, S_UNSE0=6'd4,
        S_SETTLE=6'd5, S_TOK_S=6'd6, S_GAP1=6'd7, S_DATA=6'd8, S_GAP2=6'd9,
        S_TOK_I=6'd10, S_RXW=6'd11, S_PRINT=6'd12, S_HALT=6'd13;

    reg [5:0]  st = 6'd0;
    reg [23:0] t  = 24'd0;
    reg [23:0] gwd = 24'd0;
    reg [23:0] bc = 24'd0;
    reg [5:0]  st_prev = 6'd63;

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

    reg  [7:0] u_txdata = 8'd0;
    reg        u_txval = 1'b0;
    wire       u_txrdy;
    uart_tx #(.CLK_FRE(60), .BAUD_RATE(115200)) u_tx (
        .clk(ulclk), .rst_n(ul_rst_n), .tx_pin(uart_tx),
        .tx_data(u_txdata), .tx_data_valid(u_txval),
        .tx_data_ready(u_txrdy), .tx_busy()
    );

    localparam [3:0] PID_SETUP = 4'hD, PID_DATA0 = 4'h3, PID_IN = 4'h9;

    always @(posedge ulclk or negedge ul_rst_n) begin
        if (!ul_rst_n) begin
            st <= 6'd0; r_start <= 1'b0; r_wr <= 1'b0; t_req <= 1'b0;
            t <= 24'd0; u_txval <= 1'b0; u_txdata <= 8'd0; byte_idx <= 6'd0;
            pr0<=8'h00; pr1<=8'h00; pr2<=8'h00; pr3<=8'h00; pr4<=8'h00;
            pr5<=8'h00; pr6<=8'h00; pr7<=8'h00; pr8<=8'h00;
        end else begin
            r_start <= 1'b0;
            t_req   <= 1'b0;
            t       <= 24'd0;

            case (st)
            S_INIT1:  if (por == 8'hFF) begin r_addr<=6'h04; r_wdata<=8'h4C; r_wr<=1'b1; r_start<=1'b1; st<=S_INIT2; end
            S_INIT2:  if (r_done) begin pr3 <= r_rdata; st<=S_SETTLE; end  // (bus reset skipped)
            S_SE0:    if (r_done) begin r_addr<=6'h04; r_wdata<=8'h4D; r_wr<=1'b1; r_start<=1'b1; st<=S_SE0W; end
            S_SE0W:   begin t<=t+24'd1; if (t == 24'hE0000) begin r_addr<=6'h04; r_wdata<=8'h4C; r_wr<=1'b1; r_start<=1'b1; st<=S_UNSE0; end end
            S_UNSE0:  if (r_done) begin pr4 <= r_rdata; st<=S_SETTLE; end
            S_SETTLE: begin t<=t+24'd1; if (t == 24'h20000) begin
                          t_pid<=PID_SETUP; t_len<=4'd2; t_data<=64'h0; t_req<=1'b1; st<=S_TOK_S;
                      end end
            S_TOK_S:  if (t_done) st<=S_GAP1;
            S_GAP1:   begin t<=t+24'd1; if (t==24'd63) begin
                          t_pid<=PID_DATA0; t_len<=4'd8; t_data<=64'h0008_0000_0100_0680; t_req<=1'b1; st<=S_DATA;
                      end end
            S_DATA:   if (t_done) st<=S_GAP2;
            S_GAP2:   begin t<=t+24'd1; if (t==24'd63) begin
                          t_pid<=PID_IN; t_len<=4'd2; t_data<=64'h0; t_req<=1'b1; st<=S_TOK_I;
                      end end
            S_TOK_I:  if (t_done) begin t<=24'd0; st<=S_RXW; end
            S_RXW:    begin
                          t<=t+24'd1;
                          if (rx_valid || t==24'hFFFFF) begin
                              pr0 <= {6'b00, line_state};
                              pr1 <= rx_pid;
                              pr2 <= {3'b000, rx_len};
                              pr5 <= rx_data[7:0];   pr6 <= rx_data[15:8];
                              pr7 <= rx_data[23:16]; pr8 <= rx_data[31:24];
                              byte_idx <= 6'd0; st<=S_PRINT;
                          end
                      end
            S_PRINT:  begin
                          if (u_txval) begin
                              if (u_txrdy) u_txval <= 1'b0;
                          end else if (byte_idx <= 6'd35) begin
                              u_txdata <= out_byte;
                              u_txval  <= 1'b1;
                              byte_idx <= byte_idx + 6'd1;
                          end else begin
                              byte_idx <= 6'd0; st<=S_HALT;
                          end
                      end
            S_HALT:   st <= S_HALT;
            default:  st <= 6'd0;
            endcase

            if (st != st_prev) begin st_prev <= st; gwd <= 24'd0; end
            else if (gwd != 24'hFFFFFF) gwd <= gwd + 24'd1;
            if (gwd == 24'h600000 && st != S_PRINT && st != S_HALT) begin
                pr4 <= st; pr6 <= {4'b0, dbg_state};
                byte_idx <= 6'd0; st <= S_PRINT;
            end
        end
    end

endmodule
