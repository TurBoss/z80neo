// ulpi_fcscan_top - for each candidate Function Control value: write FC,
// wait ~1 ms, read back FC(04) and Debug(15) LineState. Prints
// "vv:ff:ll " per entry (written, read back, linestate), then CR.
module ulpi_fcscan_top (
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
    assign led = 4'hF;

    reg        r_req = 1'b0, r_wr = 1'b0;
    reg  [5:0] r_addr = 6'd0;
    reg  [7:0] r_wdata = 8'd0;
    wire [7:0] r_rdata;
    wire       r_done;
    ulpi_link u_link (
        .clk(ulclk), .rst_n(ul_rst_n),
        .ulpi_data(ulpi_data), .ulpi_dir(ulpi_dir), .ulpi_nxt(ulpi_nxt), .ulpi_stp(ulpi_stp),
        .r_req(r_req), .r_wr(r_wr), .r_addr(r_addr), .r_wdata(r_wdata),
        .r_rdata(r_rdata), .r_done(r_done), .r_busy(),
        .tx_req(1'b0), .tx_pid(4'd0), .tx_len(4'd0), .tx_data(96'd0), .tx_busy(), .tx_done(),
        .rx_pid(), .rx_len(), .rx_data(), .rx_valid(), .rx_error(),
        .line_state(), .rxcmd_raw(), .rxcmd_cnt(), .tx_abort_cnt(), .tx_dir_cnt(),
        .trace_addr(10'd0), .trace_q(), .trace_done(),
        .dbg_state(), .dbg_tx_byte(), .dbg_tx_stb()
    );

    function [7:0] fcv; input [3:0] i;
        case (i)
        4'd0: fcv = 8'h41; 4'd1: fcv = 8'h45; 4'd2: fcv = 8'h46; 4'd3: fcv = 8'h40;
        4'd4: fcv = 8'h48; 4'd5: fcv = 8'h50; 4'd6: fcv = 8'h58; 4'd7: fcv = 8'h44;
        4'd8: fcv = 8'h54; 4'd9: fcv = 8'h42; 4'd10: fcv = 8'h60; default: fcv = 8'h46;
        endcase
    endfunction
    localparam N = 11;

    reg [7:0] rb [0:15];
    reg [7:0] ls [0:15];
    reg [3:0] i;
    reg [2:0] s;
    reg [19:0] w;
    reg       done;
    always @(posedge ulclk or negedge ul_rst_n) begin
        if (!ul_rst_n) begin i <= 0; s <= 0; w <= 20'd6000; done <= 0; end
        else case (s)
        3'd0: if (w != 0) w <= w - 1;
              else begin r_wr <= 1; r_addr <= 6'h0A; r_wdata <= 8'h06; r_req <= ~r_req; s <= 3'd7; end
        3'd7: if (r_done) s <= 3'd1;
        3'd1: begin r_wr <= 1; r_addr <= 6'h04; r_wdata <= fcv(i); r_req <= ~r_req; s <= 3'd2; end
        3'd2: if (r_done) begin w <= 20'd60000; s <= 3'd3; end
        3'd3: if (w != 0) w <= w - 1;
              else begin r_wr <= 0; r_addr <= 6'h04; r_req <= ~r_req; s <= 3'd4; end
        3'd4: if (r_done) begin rb[i] <= r_rdata; r_wr <= 0; r_addr <= 6'h15; r_req <= ~r_req; s <= 3'd5; end
        3'd5: if (r_done) begin
                  ls[i] <= r_rdata;
                  if (i == N-1) begin i <= 0; done <= 1; w <= 20'hFFFFF; s <= 3'd6; end
                  else begin i <= i + 1; s <= 3'd1; end
              end
        3'd6: if (w != 0) w <= w - 1; else begin done <= 0; s <= 3'd1; end
        endcase
    end

    function [7:0] nib; input [3:0] n; nib = (n < 10) ? (8'h30 + n) : (8'h37 + n); endfunction
    reg  [7:0] utx; reg uval; wire urdy;
    uart_tx #(.CLK_FRE(60), .BAUD_RATE(115200)) u (
        .clk(ulclk), .rst_n(ul_rst_n), .tx_pin(uart_tx),
        .tx_data(utx), .tx_data_valid(uval), .tx_data_ready(urdy), .tx_busy());
    reg [3:0] pi; reg [3:0] pc; reg pr;
    reg [7:0] ch;
    always @(*) begin
        case (pc)
        4'd0: ch = nib(fcv(pi) >> 4); 4'd1: ch = nib(fcv(pi));
        4'd2: ch = ":";
        4'd3: ch = nib(rb[pi][7:4]);  4'd4: ch = nib(rb[pi][3:0]);
        4'd5: ch = ":";
        4'd6: ch = nib(ls[pi][7:4]);  4'd7: ch = nib(ls[pi][3:0]);
        default: ch = (pi == N-1) ? 8'h0A : " ";
        endcase
    end
    always @(posedge ulclk or negedge ul_rst_n) begin
        if (!ul_rst_n) begin uval <= 0; pi <= 0; pc <= 0; pr <= 0; end
        else if (uval) begin if (urdy) uval <= 0; end
        else if (!pr) begin if (done) begin pr <= 1; pi <= 0; pc <= 0; end end
        else begin
            utx <= ch; uval <= 1;
            if (pc == 4'd8) begin
                pc <= 0;
                if (pi == N-1) pr <= 0; else pi <= pi + 1;
            end else pc <= pc + 1;
        end
    end
endmodule
