// ============================================================================
// Testbench for terminal_handler (NullVDP phase 2)
// ----------------------------------------------------------------------------
//   source ~/Apps/oss-cad-suite/environment
//   iverilog -o tb_terminal sim/tb_terminal.v src/terminal_handler.v && vvp tb_terminal
//
// Exercises the boot clear, plain text + default attribute, SGR colour/bold/
// inverse, cursor motion, erase, and hardware scroll address mapping.
// ============================================================================

`timescale 1ns/1ps

module tb_terminal;

    reg         clk = 0;
    reg         rst_n = 0;
    reg  [7:0]  rx_data = 0;
    reg         rx_valid = 0;

    wire        rx_ready;
    wire        char_we;
    wire [10:0] char_waddr;
    wire [7:0]  char_wdata;
    wire        attr_we;
    wire [10:0] attr_waddr;
    wire [15:0] attr_wdata;
    wire [6:0]  cursor_x;
    wire [5:0]  cursor_y;
    wire        cursor_visible;
    wire [5:0]  scroll_off;

    integer errors = 0;

    terminal_handler dut (
        .clk(clk), .rst_n(rst_n),
        .rx_data(rx_data), .rx_valid(rx_valid), .rx_ready(rx_ready),
        .char_we(char_we), .char_waddr(char_waddr), .char_wdata(char_wdata),
        .attr_we(attr_we), .attr_waddr(attr_waddr), .attr_wdata(attr_wdata),
        .cursor_x(cursor_x), .cursor_y(cursor_y),
        .cursor_visible(cursor_visible), .scroll_off(scroll_off)
    );

    always #5 clk = ~clk;   // 100 MHz sim clock (logic is synchronous)

    // latched write for the last consumed byte
    reg        last_we;
    reg [10:0] last_waddr;
    reg [7:0]  last_char;
    reg [15:0] last_attr;

    task send;
        input [7:0] b;
        begin
            @(negedge clk);
            while (!rx_ready) @(negedge clk);
            rx_data  = b;
            rx_valid = 1'b1;
            @(posedge clk); #1;
            last_we    = char_we;
            last_waddr = char_waddr;
            last_char  = char_wdata;
            last_attr  = attr_wdata;
            @(negedge clk);
            rx_valid = 1'b0;
            rx_data  = 8'h00;
            @(posedge clk); #1;   // let cursor_x/y and scroll_off registers update
        end
    endtask

    task expect_char;
        input [7:0]  code;
        input [15:0] attr;
        input [10:0] addr;
        input [127:0] label;
        begin
            if (!last_we) begin
                $display("FAIL [%0s]: no write (expected '%c')", label, code);
                errors = errors + 1;
            end else if (last_char !== code || last_attr !== attr || last_waddr !== addr) begin
                $display("FAIL [%0s]: got char=%02x attr=%04x addr=%0d, exp char=%02x attr=%04x addr=%0d",
                         label, last_char, last_attr, last_waddr, code, attr, addr);
                errors = errors + 1;
            end else
                $display("ok   [%0s]", label);
        end
    endtask

    task expect_eq;
        input [31:0] got;
        input [31:0] exp;
        input [127:0] label;
        begin
            if (got !== exp) begin
                $display("FAIL [%0s]: got %0d, exp %0d", label, got, exp);
                errors = errors + 1;
            end else
                $display("ok   [%0s] = %0d", label, got);
        end
    endtask

    integer i;
    initial begin
        // reset
        rst_n = 0;
        repeat (5) @(posedge clk);
        rst_n = 1;

        // wait out the boot full-screen clear
        @(negedge clk);
        while (!rx_ready) @(negedge clk);

        // --- plain text + default attribute at address 0 --------------------
        send("A");                    // 0x41
        expect_char(8'h41, 16'h0007, 11'd0, "plain A default attr");
        expect_eq(cursor_x, 1, "cursor x after A");

        // --- SGR red foreground --------------------------------------------
        send(8'h1B); send("["); send("3"); send("1"); send("m");
        send("B");
        expect_char(8'h42, 16'h0001, 11'd1, "B red fg");

        // --- SGR bold + red (bright) ---------------------------------------
        send(8'h1B); send("["); send("1"); send(";"); send("3"); send("1"); send("m");
        send("C");
        expect_char(8'h43, 16'h0009, 11'd2, "C bold red fg");

        // --- SGR reset ------------------------------------------------------
        send(8'h1B); send("["); send("0"); send("m");
        send("D");
        expect_char(8'h44, 16'h0007, 11'd3, "D reset attr");

        // --- SGR inverse (bit 10) ------------------------------------------
        send(8'h1B); send("["); send("7"); send("m");
        send("E");
        expect_char(8'h45, 16'h0407, 11'd4, "E inverse");

        // --- SGR blue background (44) --------------------------------------
        send(8'h1B); send("["); send("0"); send("m");
        send(8'h1B); send("["); send("4"); send("4"); send("m");
        send("F");
        expect_char(8'h46, 16'h0047, 11'd5, "F blue bg");

        // --- CR/LF ----------------------------------------------------------
        send(8'h1B); send("["); send("0"); send("m");
        send(8'h0D);                  // CR -> col 0
        expect_eq(cursor_x, 0, "cursor x after CR");
        send(8'h0A);                  // LF -> row 1
        expect_eq(cursor_y, 1, "cursor y after LF");
        send("G");
        expect_char(8'h47, 16'h0007, 11'd80, "G row1 col0");

        // --- cursor absolute: row 30, col 1 ---------------------------------
        send(8'h1B); send("["); send("3"); send("0"); send(";"); send("1"); send("H");
        expect_eq(cursor_y, 29, "cursor y after CSI 30;1H");
        expect_eq(cursor_x, 0,  "cursor x after CSI 30;1H");

        // --- scroll: LF at the bottom increments scroll_off and clears row ---
        send(8'h0A);
        // wait for the clear fill to finish
        @(negedge clk);
        while (!rx_ready) @(negedge clk);
        expect_eq(scroll_off, 1, "scroll_off after LF at bottom");

        // after scroll, writing at cursor_y=29 maps to physical row 0 -> addr 0
        send("H");
        expect_char(8'h48, 16'h0007, 11'd0, "H after scroll -> phys row 0");

        // --- cursor left/up via CSI ----------------------------------------
        send(8'h1B); send("["); send("2"); send("D");   // left 2
        expect_eq(cursor_x, 0, "cursor x after CSI 2D (clamped)");
        send(8'h1B); send("["); send("5"); send("A");   // up 5
        expect_eq(cursor_y, 24, "cursor y after CSI 5A");

        if (errors == 0) $display("\nALL TESTS PASSED");
        else             $display("\n%d TEST(S) FAILED", errors);
        $finish;
    end

    // timeout guard
    initial begin
        #2000000;
        $display("TIMEOUT");
        $finish;
    end

endmodule
