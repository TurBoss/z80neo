// ============================================================================
// ulpi_kbd_top - ULPI host bring-up top for hardware
//
// Dumps 8 bytes every ~0.28 s:
//   last_key rx_cnt tx_cnt {ls,step} host_st rx_pid bLength {det_ls,retry}
// ============================================================================
module ulpi_kbd_top (
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
    assign led = ~{dbg[39], dbg[34:32]};

    wire [63:0] dbg;
    wire [7:0]  dbg_tx_byte;
    wire        dbg_tx_stb;
    wire [7:0]  hid_key;
    wire        hid_key_valid;

    ulpi_host #(.RESET_CYCLES(900000), .SETTLE_CYCLES(3000000)) host (
        .clk(ulclk), .rst_n(ul_rst_n),
        .ulpi_data(ulpi_data), .ulpi_dir(ulpi_dir), .ulpi_nxt(ulpi_nxt),
        .ulpi_stp(ulpi_stp),
        .dbg(dbg), .dbg_tx_byte(dbg_tx_byte), .dbg_tx_stb(dbg_tx_stb),
        .hid_key(hid_key), .hid_key_valid(hid_key_valid)
    );

    // host dbg: last_key rx_cnt tx_cnt {ls,step} st rx_pid bLength {det_ls,retry}
    wire [7:0] b0 = dbg[63:56];
    wire [7:0] b1 = dbg[55:48];
    wire [7:0] b2 = dbg[47:40];
    wire [7:0] b3 = dbg[39:32];
    wire [7:0] b4 = dbg[31:24];
    wire [7:0] b5 = dbg[23:16];
    wire [7:0] b6 = dbg[15:8];
    wire [7:0] b7 = dbg[7:0];

    reg [7:0] q0,q1,q2,q3,q4,q5,q6,q7, pv, ob;
    function [7:0] pfsel; input [3:0] i;
        begin
            case (i)
            4'd0: pfsel = q0; 4'd1: pfsel = q1; 4'd2: pfsel = q2;
            4'd3: pfsel = q3; 4'd4: pfsel = q4; 4'd5: pfsel = q5;
            4'd6: pfsel = q6; default: pfsel = q7;
            endcase
        end
    endfunction
    function [7:0] nib; input [3:0] n;
        begin nib = (n < 10) ? (8'h30 + n) : (8'h37 + n); end
    endfunction

    reg [4:0] bi;
    always @(*) begin
        pv = pfsel(bi[4:1]);
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
            pst <= ST_W; uval <= 1'b0; utx <= 8'd0; bi <= 5'd0; tick <= 24'd0;
        end else begin
            tick <= tick + 24'd1;
            case (pst)
            ST_W: if (tick == 24'hFFFFFF) begin
                tick <= 24'd0;
                q0 <= b0; q1 <= b1; q2 <= b2; q3 <= b3;
                q4 <= b4; q5 <= b5; q6 <= b6; q7 <= b7;
                bi  <= 5'd0; pst <= ST_P;
            end
            ST_P: begin
                if (uval) begin
                    if (urdy) uval <= 1'b0;
                end else if (bi < 5'd16) begin
                    utx <= ob; uval <= 1'b1; bi <= bi + 5'd1;
                end else if (bi == 5'd16) begin
                    utx <= 8'h0D; uval <= 1'b1; bi <= bi + 5'd1;
                end else begin
                    pst <= ST_W;
                end
            end
            endcase
        end
    end
endmodule
