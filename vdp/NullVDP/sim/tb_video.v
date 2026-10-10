// Testbench: terminal_handler -> char/attr buffers -> text_controller render
`timescale 1ns/1ps
module tb_video;
    reg clk = 0, rst_n = 0;
    always #5 clk = ~clk;

    reg  [7:0] rx_data = 0;
    reg        rx_valid = 0;

    wire       rx_ready;
    wire       char_we, attr_we;
    wire [11:0] char_waddr, attr_waddr;
    wire [7:0]  char_wdata;
    wire [15:0] attr_wdata;
    wire [6:0]  cursor_x;
    wire [5:0]  cursor_y;
    wire        cursor_vis;
    wire [5:0]  scroll_off;

    wire [11:0] char_addr, attr_addr;
    wire [7:0]  char_data;
    wire [15:0] attr_data;
    wire        hsync, vsync, de;
    wire [7:0]  red, green, blue;

    terminal_handler u_th (
        .clk(clk), .rst_n(rst_n),
        .rx_data(rx_data), .rx_valid(rx_valid), .rx_ready(rx_ready),
        .char_we(char_we), .char_waddr(char_waddr), .char_wdata(char_wdata),
        .attr_we(attr_we), .attr_waddr(attr_waddr), .attr_wdata(attr_wdata),
        .cursor_x(cursor_x), .cursor_y(cursor_y),
        .cursor_visible(cursor_vis), .scroll_off(scroll_off)
    );

    char_buffer u_cb (
        .clk_rd(clk), .rd_addr(char_addr), .rd_data(char_data),
        .clk_wr(clk), .wr_en(char_we), .wr_addr(char_waddr), .wr_data(char_wdata)
    );
    attr_buffer u_ab (
        .clk_rd(clk), .rd_addr(attr_addr), .rd_data(attr_data),
        .clk_wr(clk), .wr_en(attr_we), .wr_addr(attr_waddr), .wr_data(attr_wdata)
    );

    text_controller u_tc (
        .clk(clk), .rst_n(rst_n),
        .cursor_x(cursor_x), .cursor_y(cursor_y),
        .cursor_visible(cursor_vis), .scroll_off(scroll_off),
        .char_addr(char_addr), .char_data(char_data),
        .attr_addr(attr_addr), .attr_data(attr_data),
        .hsync(hsync), .vsync(vsync), .de(de),
        .red(red), .green(green), .blue(blue)
    );

    integer nonblack = 0, pixels = 0;
    reg     seen_a_row = 0;

    initial begin
        repeat (5) @(posedge clk);
        rst_n = 1;
        // wait out the boot clear
        @(negedge clk);
        while (!rx_ready) @(negedge clk);
        repeat (20) @(posedge clk);

        // send 'A'
        @(negedge clk); rx_data = 8'h41; rx_valid = 1'b1;
        @(posedge clk); #1;
        $display("write: char_we=%b waddr=%0d wdata=%02x attr_we=%b attr=%04x cursor=(%0d,%0d)",
                 char_we, char_waddr, char_wdata, attr_we, attr_wdata, cursor_x, cursor_y);
        @(negedge clk); rx_valid = 1'b0;

        // scan one full frame's active pixels
        for (int i = 0; i < 800*525; i++) begin
            @(posedge clk); #1;
            if (de) begin
                pixels++;
                if (red || green || blue) begin
                    nonblack++;
                    if (red == 8'hAA && green == 8'hAA && blue == 8'hAA) seen_a_row = 1'b1;
                end
            end
        end
        $display("active pixels=%0d nonblack=%0d saw_grey(A fg)=%b", pixels, nonblack, seen_a_row);
        $finish;
    end

    // sample the buffer content at address 0 shortly after the write
    reg [7:0] ch0; reg [15:0] at0;
    initial begin
        #1;
        @(posedge char_we);
        ch0 = char_wdata; at0 = attr_wdata;
        $display("captured write char=%02x attr=%04x", ch0, at0);
    end

endmodule
