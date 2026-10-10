// ============================================================================
// ulpi_regdump_top - read USB3317 registers 0x00..0x0F on hardware and print
//   them every ~0.5 s:  <r00> <r01> ... <r0F> CR
// Confirms whether register READS work and exposes the PHY's own state
// (vendor/product IDs, Function Control, OTG Control, and any status bits).
// ============================================================================
module ulpi_regdump_top (
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

    reg        r_req = 1'b0;
    reg        r_wr  = 1'b0;
    reg  [5:0] r_addr = 6'd0;
    reg  [7:0] r_wdata = 8'd0;
    wire [7:0] r_rdata;
    wire       r_done;

    ulpi_link u_link (
        .clk(ulclk), .rst_n(ul_rst_n),
        .ulpi_data(ulpi_data), .ulpi_dir(ulpi_dir), .ulpi_nxt(ulpi_nxt),
        .ulpi_stp(ulpi_stp),
        .r_req(r_req), .r_wr(r_wr), .r_addr(r_addr), .r_wdata(r_wdata),
        .r_rdata(r_rdata), .r_done(r_done), .r_busy(),
        .tx_req(1'b0), .tx_pid(4'd0), .tx_len(4'd0), .tx_data(96'd0),
        .tx_busy(), .tx_done(),
        .rx_pid(), .rx_len(), .rx_data(), .rx_valid(), .rx_error(),
        .line_state(), .dbg_state(),
        .dbg_tx_byte(), .dbg_tx_stb()
    );

    reg [7:0] regs [0:15];
    reg [3:0] idx = 4'd0;
    reg [1:0] fst = 2'd0;
    reg [7:0] gap = 8'd0;

    always @(posedge ulclk or negedge ul_rst_n) begin
        if (!ul_rst_n) begin
            r_req <= 1'b0; idx <= 4'd0; fst <= 2'd0; gap <= 8'd0;
        end else begin
            case (fst)
            2'd0: if (gap == 8'd0) begin
                      r_wr <= 1'b0; r_addr <= {2'b0, idx}; r_req <= ~r_req; fst <= 2'd1;
                  end else gap <= gap - 8'd1;
            2'd1: if (r_done) begin regs[idx] <= r_rdata; fst <= 2'd2; end
            2'd2: begin
                      if (idx == 4'd15) idx <= 4'd0; else idx <= idx + 4'd1;
                      gap <= 8'd40; fst <= 2'd0;
                  end
            default: fst <= 2'd0;
            endcase
        end
    end

    // ---- printer: 16 hex bytes + CR every ~0.5 s ----
    reg [7:0] q0,q1,q2,q3,q4,q5,q6,q7,q8,q9,q10,q11,q12,q13,q14,q15,q16,q17;
    reg [7:0] pv, ob;
    function [7:0] pfsel; input [4:0] i;
        begin
            case (i)
            5'd0:pfsel=q0;  5'd1:pfsel=q1;  5'd2:pfsel=q2;  5'd3:pfsel=q3;
            5'd4:pfsel=q4;  5'd5:pfsel=q5;  5'd6:pfsel=q6;  5'd7:pfsel=q7;
            5'd8:pfsel=q8;  5'd9:pfsel=q9;  5'd10:pfsel=q10;5'd11:pfsel=q11;
            5'd12:pfsel=q12;5'd13:pfsel=q13;5'd14:pfsel=q14;5'd15:pfsel=q15;
            default:pfsel=q16;
            endcase
        end
    endfunction
    function [7:0] nib; input [3:0] n;
        begin nib = (n < 10) ? (8'h30 + n) : (8'h37 + n); end
    endfunction
    reg [5:0] bi;
    always @(*) begin
        pv = pfsel(bi[5:1]);
        ob = bi[0] ? nib(pv[3:0]) : nib(pv[7:4]);
    end

    reg  [7:0] utx = 8'd0;
    reg        uval = 1'b0;
    wire       urdy;
    uart_tx #(.CLK_FRE(60), .BAUD_RATE(115200)) u (
        .clk(ulclk), .rst_n(ul_rst_n), .tx_pin(uart_tx),
        .tx_data(utx), .tx_data_valid(uval), .tx_data_ready(urdy), .tx_busy()
    );

    localparam ST_W = 1'b0, ST_P = 1'b1;
    reg        pst = ST_W;
    reg [23:0] tick = 24'd0;

    always @(posedge ulclk or negedge ul_rst_n) begin
        if (!ul_rst_n) begin
            pst <= ST_W; uval <= 1'b0; utx <= 8'd0; bi <= 6'd0; tick <= 24'd0;
        end else begin
            tick <= tick + 24'd1;
            case (pst)
            ST_W: if (tick == 24'h1FFFFFF) begin
                tick <= 24'd0;
                q0<=regs[0]; q1<=regs[1]; q2<=regs[2];  q3<=regs[3];
                q4<=regs[4]; q5<=regs[5]; q6<=regs[6];  q7<=regs[7];
                q8<=regs[8]; q9<=regs[9]; q10<=regs[10];q11<=regs[11];
                q12<=regs[12];q13<=regs[13];q14<=regs[14];q15<=regs[15];
                bi  <= 6'd0; pst <= ST_P;
            end
            ST_P: begin
                if (uval) begin
                    if (urdy) uval <= 1'b0;
                end else if (bi < 6'd32) begin
                    utx <= ob; uval <= 1'b1; bi <= bi + 6'd1;
                end else if (bi == 6'd32) begin
                    utx <= 8'h0D; uval <= 1'b1; bi <= bi + 6'd1;
                end else begin
                    pst <= ST_W;
                end
            end
            endcase
        end
    end
endmodule
