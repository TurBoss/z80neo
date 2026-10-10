// ============================================================================
// ulpi_diag_top - host-mode PHY status probe (USB3317).
//   init : OTG Ctrl(0A)=OTGV, Function Ctrl(04)=FCV, Scratch(16)=A5
//   loop : read 04 07 0A 13 14 15 16, print with last RXCMD + RXCMD count:
//          FC IFC OTG ISTAT ILATCH DEBUG SCRATCH RXCMD RXCNT CR
//   0x13 bits: 0 HostDisconnect 1 VbusValid 2 SessValid 3 SessEnd 4 IdGnd
//   0x15 bits: 1:0 LineState (1=J/D+ high, 2=K/D- high, 0=SE0)
// ============================================================================
module ulpi_diag_top #(
    parameter [7:0] OTGV = 8'h06,
    parameter [7:0] FCV  = 8'h45,
    parameter [2:0] INIT = 3'b111,   // bit0 OTG, bit1 FC, bit2 scratch
    parameter       SHOW = 0         // 0 = register values, 1 = LineState histogram
) (
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

    reg        r_req = 1'b0;
    reg        r_wr  = 1'b0;
    reg  [5:0] r_addr = 6'd0;
    reg  [7:0] r_wdata = 8'd0;
    wire [7:0] r_rdata;
    wire       r_done;
    wire [7:0] rxcmd_raw, rxcmd_cnt;
    wire [3:0] lst;

    ulpi_link u_link (
        .clk(ulclk), .rst_n(ul_rst_n),
        .ulpi_data(ulpi_data), .ulpi_dir(ulpi_dir), .ulpi_nxt(ulpi_nxt),
        .ulpi_stp(ulpi_stp),
        .r_req(r_req), .r_wr(r_wr), .r_addr(r_addr), .r_wdata(r_wdata),
        .r_rdata(r_rdata), .r_done(r_done), .r_busy(),
        .tx_req(1'b0), .tx_pid(4'd0), .tx_len(4'd0), .tx_data(96'd0),
        .tx_busy(), .tx_done(),
        .rx_pid(), .rx_len(), .rx_data(), .rx_valid(), .rx_error(),
        .line_state(), .rxcmd_raw(rxcmd_raw), .rxcmd_cnt(rxcmd_cnt),
        .dbg_state(lst), .dbg_tx_byte(), .dbg_tx_stb()
    );

    // op table: 0..2 = init writes, 3..9 = reads (loop over 3..9)
    reg [3:0] op = 4'd0;
    always @(*) begin
        case (op)
        4'd0: begin r_wr = 1'b1; r_addr = 6'h0A; r_wdata = OTGV;  end
        4'd1: begin r_wr = 1'b1; r_addr = 6'h04; r_wdata = FCV;   end
        4'd2: begin r_wr = 1'b1; r_addr = 6'h16; r_wdata = 8'hA5; end
        4'd3: begin r_wr = 1'b0; r_addr = 6'h04; r_wdata = 8'h00; end
        4'd4: begin r_wr = 1'b0; r_addr = 6'h07; r_wdata = 8'h00; end
        4'd5: begin r_wr = 1'b0; r_addr = 6'h0A; r_wdata = 8'h00; end
        4'd6: begin r_wr = 1'b0; r_addr = 6'h13; r_wdata = 8'h00; end
        4'd7: begin r_wr = 1'b0; r_addr = 6'h14; r_wdata = 8'h00; end
        4'd8: begin r_wr = 1'b0; r_addr = 6'h15; r_wdata = 8'h00; end
        default: begin r_wr = 1'b0; r_addr = 6'h16; r_wdata = 8'h00; end
        endcase
    end

    reg [7:0] v4 = 0, v7 = 0, vA = 0, v13 = 0, v14 = 0, v15 = 0, v16 = 0;
    reg [1:0] fst = 2'd0;
    reg [7:0] gap = 8'd200;

    always @(posedge ulclk or negedge ul_rst_n) begin
        if (!ul_rst_n) begin
            r_req <= 1'b0; op <= 4'd0; fst <= 2'd0; gap <= 8'd200;
        end else begin
            case (fst)
            2'd0: if (op <= 4'd2 && !INIT[op[1:0]]) begin op <= op + 4'd1; end
                  else if (gap == 8'd0) begin r_req <= ~r_req; fst <= 2'd1; end
                  else gap <= gap - 8'd1;
            2'd1: if (r_done) begin
                      case (op)
                      4'd3: v4  <= r_rdata;  4'd4: v7  <= r_rdata;
                      4'd5: vA  <= r_rdata;  4'd6: v13 <= r_rdata;
                      4'd7: v14 <= r_rdata;  4'd8: v15 <= r_rdata;
                      4'd9: v16 <= r_rdata;  default: ;
                      endcase
                      fst <= 2'd2;
                  end
            2'd2: begin
                      if (op == 4'd2 && SHOW != 0) fst <= 2'd3;   // leave the bus alone
                      else begin
                          op  <= (op == 4'd9) ? 4'd3 : op + 4'd1;
                          gap <= 8'd40; fst <= 2'd0;
                      end
                  end
            default: ;
            endcase
        end
    end

    // time spent in each LineState (from RXCMD) per print window, in 256-cycle units
    reg [23:0] c0, c1, c2, c3;
    reg [23:0] h0, h1, h2, h3;
    reg [7:0]  cnt0, cntw;
    wire       wend;
    always @(posedge ulclk) begin
        if (wend) begin
            h0 <= c0; h1 <= c1; h2 <= c2; h3 <= c3;
            c0 <= 0; c1 <= 0; c2 <= 0; c3 <= 0;
            cntw <= rxcmd_cnt - cnt0; cnt0 <= rxcmd_cnt;
        end else case (rxcmd_raw[1:0])
            2'd0: c0 <= c0 + 1; 2'd1: c1 <= c1 + 1;
            2'd2: c2 <= c2 + 1; default: c3 <= c3 + 1;
        endcase
    end

    assign led = {~v15[1], ~v15[0], ~v13[1], ~v13[0]};

    // ---- printer: 9 hex bytes + CR ----
    reg [7:0] q0,q1,q2,q3,q4,q5,q6,q7,q8;
    reg [7:0] pv, ob;
    reg [4:0] bi;
    always @(*) begin
        case (bi[4:1])
        4'd0: pv = q0; 4'd1: pv = q1; 4'd2: pv = q2; 4'd3: pv = q3;
        4'd4: pv = q4; 4'd5: pv = q5; 4'd6: pv = q6; 4'd7: pv = q7;
        default: pv = q8;
        endcase
        ob = bi[0] ? ((pv[3:0] < 10) ? 8'h30 + pv[3:0] : 8'h37 + pv[3:0])
                   : ((pv[7:4] < 10) ? 8'h30 + pv[7:4] : 8'h37 + pv[7:4]);
    end

    reg  [7:0] utx = 8'd0;
    reg        uval = 1'b0;
    wire       urdy;
    uart_tx #(.CLK_FRE(60), .BAUD_RATE(115200)) u (
        .clk(ulclk), .rst_n(ul_rst_n), .tx_pin(uart_tx),
        .tx_data(utx), .tx_data_valid(uval), .tx_data_ready(urdy), .tx_busy()
    );

    reg        pst = 1'b0;
    reg [23:0] tick = 24'd0;
    assign wend = (tick == 24'hFFFFFE);
    always @(posedge ulclk or negedge ul_rst_n) begin
        if (!ul_rst_n) begin
            pst <= 1'b0; uval <= 1'b0; bi <= 5'd0; tick <= 24'd0;
        end else begin
            tick <= tick + 24'd1;
            if (!pst) begin
                if (tick == 24'hFFFFFF) begin
                    if (SHOW != 0) begin
                        q0 <= h0[23:16]; q1 <= h0[15:8]; q2 <= h1[23:16]; q3 <= h1[15:8];
                        q4 <= h2[23:16]; q5 <= h2[15:8]; q6 <= h3[23:16];
                    end else begin
                        q0 <= v4; q1 <= v7; q2 <= vA; q3 <= v13; q4 <= v14; q5 <= v15; q6 <= v16;
                    end
                    q7 <= rxcmd_raw; q8 <= {op, fst, lst[1:0]};
                    bi <= 5'd0; pst <= 1'b1;
                end
            end else begin
                if (uval) begin
                    if (urdy) uval <= 1'b0;
                end else if (bi < 5'd18) begin
                    utx <= ob; uval <= 1'b1; bi <= bi + 5'd1;
                end else if (bi == 5'd18) begin
                    utx <= 8'h0D; uval <= 1'b1; bi <= bi + 5'd1;
                end else pst <= 1'b0;
            end
        end
    end
endmodule
