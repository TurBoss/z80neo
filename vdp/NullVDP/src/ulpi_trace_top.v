// ulpi_trace_top - run ulpi_host, dump the 256-cycle ULPI trace captured from
// the first SETUP transmit: one line per cycle "DNSO DD" (dir nxt stp oe, data).
module ulpi_trace_top (
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

    reg  [9:0]  taddr = 10'd0;
    wire [31:0] tq;
    wire        tdone;
    ulpi_host #(.RESET_CYCLES(900000), .SETTLE_CYCLES(3000000)) host (
        .clk(ulclk), .rst_n(ul_rst_n),
        .ulpi_data(ulpi_data), .ulpi_dir(ulpi_dir), .ulpi_nxt(ulpi_nxt),
        .ulpi_stp(ulpi_stp),
        .dbg(), .dbg_tx_byte(), .dbg_tx_stb(),
        .trace_addr(taddr), .trace_q(tq), .trace_done(tdone),
        .hid_key(), .hid_key_valid()
    );
    assign led = {3'b111, ~tdone};

    reg  [7:0] utx = 8'd0;
    reg        uval = 1'b0;
    wire       urdy;
    uart_tx #(.CLK_FRE(60), .BAUD_RATE(115200)) u (
        .clk(ulclk), .rst_n(ul_rst_n), .tx_pin(uart_tx),
        .tx_data(utx), .tx_data_valid(uval), .tx_data_ready(urdy), .tx_busy()
    );
    function [7:0] nib; input [3:0] n;
        begin nib = (n < 10) ? (8'h30 + n) : (8'h37 + n); end
    endfunction
    // per entry: "DNSO RT TTT DD\n"  (flags, rstate/tstate, delta cycles, data)
    reg [31:0] e;
    reg [3:0]  ci;
    reg [1:0]  ph;           // 0 = fetch wait, 1 = latch, 2 = print
    reg [25:0] pause = 26'd0;
    reg        run = 1'b0;
    reg [7:0]  ch;
    always @(*) begin
        case (ci)
        4'd0:  ch = e[11] ? "D" : ".";
        4'd1:  ch = e[10] ? "N" : ".";
        4'd2:  ch = e[9]  ? "S" : ".";
        4'd3:  ch = e[8]  ? "O" : ".";
        4'd4:  ch = " ";
        4'd5:  ch = nib(e[19:16]);
        4'd6:  ch = nib(e[15:12]);
        4'd7:  ch = " ";
        4'd8:  ch = nib(e[31:28]);
        4'd9:  ch = nib(e[27:24]);
        4'd10: ch = nib(e[23:20]);
        4'd11: ch = " ";
        4'd12: ch = nib(e[7:4]);
        4'd13: ch = nib(e[3:0]);
        default: ch = 8'h0A;
        endcase
    end
    always @(posedge ulclk or negedge ul_rst_n) begin
        if (!ul_rst_n) begin uval <= 1'b0; ci <= 0; ph <= 0; taddr <= 0; run <= 1'b0; pause <= 0; end
        else if (uval) begin if (urdy) uval <= 1'b0; end
        else if (!run) begin
            if (tdone) begin
                if (pause == 26'h3FFFFFF) begin run <= 1'b1; taddr <= 0; ph <= 0; pause <= 0; end
                else pause <= pause + 1;
            end
        end else case (ph)
            2'd0: ph <= 2'd1;                              // tq follows taddr by one clock
            2'd1: begin e <= tq; ci <= 4'd0; ph <= 2'd2; end
            default: begin
                utx <= ch; uval <= 1'b1;
                if (ci == 4'd14) begin
                    if (taddr == 10'd1023) run <= 1'b0;
                    else begin taddr <= taddr + 10'd1; ph <= 2'd0; end
                end else ci <= ci + 4'd1;
            end
        endcase
    end
endmodule
