// ============================================================================
// tb_gfx - ESC g graphics path: terminal_handler -> gfx_engine -> gfx_buffer
//
// Drives console bytes for each Z80NEO_GFX_* command and inspects the three
// bitplanes through a hierarchical reference.
// ============================================================================
`timescale 1ns/1ps

module tb_gfx;

    reg clk = 0, rst_n = 0;
    always #5 clk = ~clk;

    // console side
    reg  [7:0] rx_data = 0;
    reg        rx_valid = 0;
    wire       rx_ready;

    // handler outputs
    wire        char_we, attr_we;
    wire [11:0] char_waddr, attr_waddr;
    wire [7:0]  char_wdata;
    wire [15:0] attr_wdata;
    wire [6:0]  cursor_x;
    wire [5:0]  cursor_y;
    wire        cursor_visible;
    wire [5:0]  scroll_off;
    wire        key_toggle, key_up, key_down, key_enter, key_esc;
    wire        gfx_active;
    wire [2:0]  gfx_fg, gfx_bg;
    wire        gfx_start;
    wire [1:0]  gfx_shape;
    wire [2:0]  gfx_colour;
    wire [10:0] gfx_ax, gfx_bx;
    wire [8:0]  gfx_ay, gfx_by;
    wire        gfx_busy;

    // engine -> buffer
    wire [2:0]  gfx_we, gfx_wdata;
    wire [17:0] gfx_waddr;
    wire [2:0]  gfx_rd_data;

    terminal_handler th (
        .clk(clk), .rst_n(rst_n),
        .rx_data(rx_data), .rx_valid(rx_valid), .rx_ready(rx_ready),
        .char_we(char_we), .char_waddr(char_waddr), .char_wdata(char_wdata),
        .attr_we(attr_we), .attr_waddr(attr_waddr), .attr_wdata(attr_wdata),
        .cursor_x(cursor_x), .cursor_y(cursor_y),
        .cursor_visible(cursor_visible), .scroll_off(scroll_off),
        .key_toggle(key_toggle), .key_up(key_up), .key_down(key_down),
        .key_enter(key_enter), .key_esc(key_esc),
        .gfx_active(gfx_active), .gfx_fg(gfx_fg), .gfx_bg(gfx_bg),
        .gfx_start(gfx_start), .gfx_shape(gfx_shape), .gfx_colour(gfx_colour),
        .gfx_ax(gfx_ax), .gfx_ay(gfx_ay), .gfx_bx(gfx_bx), .gfx_by(gfx_by),
        .gfx_busy(gfx_busy)
    );

    gfx_engine eng (
        .clk(clk), .rst_n(rst_n),
        .start(gfx_start), .shape(gfx_shape), .colour(gfx_colour),
        .ax(gfx_ax), .ay(gfx_ay), .bx(gfx_bx), .by(gfx_by),
        .busy(gfx_busy),
        .we(gfx_we), .waddr(gfx_waddr), .wdata(gfx_wdata)
    );

    gfx_buffer gb (
        .clk_rd(clk), .rd_addr(18'd0), .rd_data(gfx_rd_data),
        .clk_wr(clk), .we(gfx_we), .wr_addr(gfx_waddr), .wr_data(gfx_wdata)
    );

    integer errors = 0;
    task chk;
        input        cond;
        input [255:0] msg;
        begin
            if (!cond) begin
                $display("FAIL: %0s", msg);
                errors = errors + 1;
            end
        end
    endtask

    // send one console byte, honouring the handler's ready gate
    task send;
        input [7:0] b;
        begin
            @(posedge clk);
            while (!rx_ready) @(posedge clk);
            rx_data  <= b;
            rx_valid <= 1'b1;
            @(posedge clk);
            rx_valid <= 1'b0;
            rx_data  <= 8'h00;
        end
    endtask

    task wait_engine;
        begin
            repeat (4) @(posedge clk);
            while (gfx_busy) @(posedge clk);
            repeat (2) @(posedge clk);
        end
    endtask

    // ESC 'g' <cmd> <payload...>
    task gfx_cmd;
        input [7:0] cmd;
        input [7:0] p0, p1, p2, p3, p4, p5, p6, p7, p8;
        input integer n;                       // payload length
        begin
            send(8'h1B); send(8'h67); send(cmd);
            if (n > 0) send(p0);
            if (n > 1) send(p1);
            if (n > 2) send(p2);
            if (n > 3) send(p3);
            if (n > 4) send(p4);
            if (n > 5) send(p5);
            if (n > 6) send(p6);
            if (n > 7) send(p7);
            if (n > 8) send(p8);
            wait_engine;
        end
    endtask

    integer k;
    initial begin
        repeat (4) @(posedge clk);
        rst_n = 1;
        repeat (4) @(posedge clk);

        // --- mode on (clears bitmap) ---------------------------------------
        gfx_cmd(8'h01, 8'h01, 0,0,0,0,0,0,0,0, 1);
        chk(gfx_active === 1'b1, "mode on sets gfx_active");

        // --- set colours fg=12(->red idx6) bg=0 ----------------------------
        gfx_cmd(8'h03, 8'h0C, 8'h00, 0,0,0,0,0,0,0, 2);
        chk(gfx_fg === 3'd6, "fg colour mapped (12 -> red)");
        chk(gfx_bg === 3'd0, "bg colour 0");

        // --- pixel (10,20) on: fg=6 (110) -> plane2+plane1 -----------------
        gfx_cmd(8'h04, 8'h0A, 8'h00, 8'h14, 8'h00, 8'h01, 0,0,0,0, 5);
        chk(gb.p2[20*640+10] === 1'b1, "pixel fg set plane2");
        chk(gb.p1[20*640+10] === 1'b1, "pixel fg set plane1");
        chk(gb.p0[20*640+10] === 1'b0, "pixel fg clear plane0");

        // --- pixel (100,50) explicit colour 7 -> mapped to idx 2 (010) -----
        gfx_cmd(8'h07, 8'h64, 8'h00, 8'h32, 8'h00, 8'h07, 0,0,0,0, 5);
        chk(gb.p0[50*640+100] === 1'b0, "pixel_c colour7 plane0");
        chk(gb.p1[50*640+100] === 1'b1, "pixel_c colour7 plane1");
        chk(gb.p2[50*640+100] === 1'b0, "pixel_c colour7 plane2");

        // --- line (0,0)-(5,0) on -------------------------------------------
        gfx_cmd(8'h05, 8'h00,8'h00,8'h00,8'h00, 8'h05,8'h00,8'h00,8'h00, 8'h01, 9);
        for (k = 0; k <= 5; k = k + 1)
            chk(gb.p2[k] === 1'b1, "line plots every pixel");

        // --- fill x=200 y=10 w=4 h=2 on ------------------------------------
        gfx_cmd(8'h06, 8'hC8,8'h00,8'h0A,8'h00, 8'h04,8'h00,8'h02,8'h00, 8'h01, 9);
        chk(gb.p2[10*640+200] === 1'b1, "fill top-left");
        chk(gb.p2[10*640+203] === 1'b1, "fill top-right");
        chk(gb.p2[11*640+200] === 1'b1, "fill bottom-left");
        chk(gb.p2[12*640+200] === 1'b0, "fill does not overrun height");

        // --- clear to bg=0 wipes a previously set pixel ---------------------
        gfx_cmd(8'h02, 8'h00, 0,0,0,0,0,0,0,0, 1);
        chk(gb.p2[50*640+100] === 1'b0, "clear wipes the bitmap");

        // --- mode off ------------------------------------------------------
        gfx_cmd(8'h01, 8'h00, 0,0,0,0,0,0,0,0, 1);
        chk(gfx_active === 1'b0, "mode off clears gfx_active");

        if (errors == 0) $display("PASS: all gfx checks");
        else             $display("FAILURES: %0d", errors);
        $finish;
    end

    initial begin
        #20000000;
        $display("TIMEOUT");
        $finish;
    end

endmodule
