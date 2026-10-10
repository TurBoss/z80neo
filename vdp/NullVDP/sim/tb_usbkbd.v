// ============================================================================
// tb_usbkbd - USB HID key -> console byte path
//   * usb_key_cdc crosses one key event from the ULPI clock into 27 MHz
//   * hid_expand maps HID usage + shift to the console byte(s)
// ============================================================================
`timescale 1ns/1ps
module tb_usbkbd;
    // 27 MHz console clock / ~60 MHz ULPI clock
    reg clk = 0;   always #18.5 clk  = ~clk;
    reg uclk = 0;  always #8.33 uclk = ~uclk;
    wire ulclk = ~uclk;

    reg  [7:0] key_in, mod_in;
    reg        valid_in;
    reg        rst_n = 0;
    wire [7:0] ckey, cmod;
    wire       cvalid;

    usb_key_cdc dut_cdc (
        .clk_in(ulclk), .rst_in_n(rst_n),
        .key_in(key_in), .mod_in(mod_in), .valid_in(valid_in),
        .clk_out(clk), .rst_out_n(rst_n),
        .key_out(ckey), .mod_out(cmod), .valid_out(cvalid)
    );

    wire [7:0] out;
    wire       outv;
    hid_expand dut_hx (
        .clk(clk), .rst_n(rst_n),
        .key(ckey), .mod(cmod), .key_valid(cvalid),
        .chr(out), .chr_valid(outv), .ready(1'b1)
    );

    integer errors = 0;
    reg [7:0] cap [0:15];
    integer   ncap;
    always @(posedge clk) begin
        if (outv) begin cap[ncap] = out; ncap = ncap + 1; end
    end

    integer i;
    always @(posedge clk) if (cvalid) begin
        $display("  cdc: key=%02X mod=%02X", ckey, cmod);
    end

    task press;
        input [7:0] k;
        input [7:0] m;
        begin
            @(posedge ulclk); #1;
            key_in = k; mod_in = m; valid_in = 1'b1;
            @(posedge ulclk); #1;
            valid_in = 1'b0;
        end
    endtask

    task expect_bytes;
        input integer n;
        begin
            while (ncap < n) @(posedge clk);
            @(posedge clk);
        end
    endtask

    task check;
        input [7:0] e0, e1, e2, e3, e4;
        input integer n;
        begin
            expect_bytes(n);
            if (cap[0] !== e0) begin $display("FAIL byte0 %02X want %02X", cap[0], e0); errors=errors+1; end
            if (n > 1 && cap[1] !== e1) begin $display("FAIL byte1 %02X want %02X", cap[1], e1); errors=errors+1; end
            if (n > 2 && cap[2] !== e2) begin $display("FAIL byte2 %02X want %02X", cap[2], e2); errors=errors+1; end
            if (n > 3 && cap[3] !== e3) begin $display("FAIL byte3 %02X want %02X", cap[3], e3); errors=errors+1; end
            if (n > 4 && cap[4] !== e4) begin $display("FAIL byte4 %02X want %02X", cap[4], e4); errors=errors+1; end
        end
    endtask

    initial begin
        key_in = 8'h00; mod_in = 8'h00; valid_in = 1'b0; ncap = 0;
        repeat (10) @(posedge ulclk);
        rst_n = 1'b1;
        repeat (10) @(posedge ulclk);

        ncap = 0; press(8'h04, 8'h00);            // 'a'
        check(8'h61,0,0,0,0,1); $display("ok:   a -> 61");
        ncap = 0; press(8'h04, 8'h02);            // shift-A
        check(8'h41,0,0,0,0,1); $display("ok:   shift-a -> 41");
        ncap = 0; press(8'h1E, 8'h02);            // '!'
        check(8'h21,0,0,0,0,1); $display("ok:   shift-1 -> 21");
        ncap = 0; press(8'h28, 8'h00);            // Enter
        check(8'h0D,0,0,0,0,1); $display("ok:   Enter -> 0D");
        ncap = 0; press(8'h29, 8'h00);            // Esc
        check(8'h1B,0,0,0,0,1); $display("ok:   Esc -> 1B");
        ncap = 0; press(8'h2C, 8'h00);            // Space
        check(8'h20,0,0,0,0,1); $display("ok:   Space -> 20");
        ncap = 0; press(8'h52, 8'h00);            // Up -> ESC [ A
        check(8'h1B,8'h5B,8'h41,0,0,3); $display("ok:   Up -> 1B 5B 41");
        ncap = 0; press(8'h45, 8'h00);            // F12 -> ESC [ 2 4 ~
        check(8'h1B,8'h5B,8'h32,8'h34,8'h7E,5); $display("ok:   F12 -> 1B 5B 32 34 7E");

        if (errors == 0) $display("USB KBD TB PASSED");
        else             $display("USB KBD TB FAILED (%0d)", errors);
        $finish;
    end
endmodule
