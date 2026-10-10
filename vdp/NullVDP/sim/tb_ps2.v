// ============================================================================
// tb_ps2 - drive synthetic scan-code-set-2 frames through ps2_host and check
//          ps2_keys / ps2_ascii decode them correctly.
// ============================================================================
`timescale 1ns/1ps
module tb_ps2;
    reg clk = 0;
    always #5 clk = ~clk;          // 100 MHz

    reg rst_n = 0;
    reg ps2_clk = 1, ps2_data = 1;

    wire [7:0] scancode;
    wire       sc_valid, sc_break, sc_ext, frame_err;
    wire       key_toggle, key_up, key_down, key_enter, key_esc;
    wire [7:0] ascii;
    wire       ascii_valid;

    ps2_host u_host (
        .clk(clk), .rst_n(rst_n), .ps2_clk(ps2_clk), .ps2_data(ps2_data),
        .scancode(scancode), .sc_valid(sc_valid), .sc_break(sc_break),
        .sc_ext(sc_ext), .frame_err(frame_err)
    );
    ps2_keys u_keys (
        .clk(clk), .rst_n(rst_n), .scancode(scancode), .sc_valid(sc_valid),
        .sc_break(sc_break), .sc_ext(sc_ext),
        .key_toggle(key_toggle), .key_up(key_up), .key_down(key_down),
        .key_enter(key_enter), .key_esc(key_esc)
    );
    ps2_ascii u_ascii (
        .clk(clk), .rst_n(rst_n), .scancode(scancode), .sc_valid(sc_valid),
        .sc_break(sc_break), .sc_ext(sc_ext),
        .ascii_data(ascii), .ascii_valid(ascii_valid)
    );

    // Single clocked monitor: counters latch every one-cycle pulse.
    reg       clr;
    integer   n_ascii, n_toggle, n_up, n_down, n_enter, n_esc;
    reg [7:0] last_ascii;
    always @(posedge clk) begin
        if (clr) begin
            n_ascii <= 0; n_toggle <= 0; n_up <= 0; n_down <= 0;
            n_enter <= 0; n_esc <= 0;
        end else begin
            if (ascii_valid) begin n_ascii <= n_ascii + 1; last_ascii <= ascii; end
            if (key_toggle)  n_toggle <= n_toggle + 1;
            if (key_up)      n_up     <= n_up + 1;
            if (key_down)    n_down   <= n_down + 1;
            if (key_enter)   n_enter  <= n_enter + 1;
            if (key_esc)     n_esc    <= n_esc + 1;
        end
    end

    task do_clear; begin
        clr <= 1'b1; @(posedge clk);
        clr <= 1'b0; @(posedge clk);
    end endtask

    integer errors = 0;

    task send_bit(input b); begin
        ps2_data = b;
        #2000 ps2_clk = 0;
        #2000 ps2_clk = 1;
    end endtask

    task send_byte(input [7:0] d);
        integer i; reg p; begin
            p = 1'b1;
            send_bit(1'b0);                 // start
            for (i = 0; i < 8; i = i + 1) begin
                send_bit(d[i]);
                p = p ^ d[i];
            end
            send_bit(p);                    // odd parity
            send_bit(1'b1);                 // stop
        end
    endtask

    task check(input cond, input [255:0] msg);
        begin
            if (!cond) begin $display("FAIL: %0s", msg); errors = errors + 1; end
            else       $display("ok:   %0s", msg);
        end
    endtask

    initial begin
        if ($test$plusargs("vcd")) begin
            $dumpfile("tb_ps2.vcd"); $dumpvars(0, tb_ps2);
        end
        repeat (10) @(posedge clk);
        rst_n = 1;
        repeat (10) @(posedge clk);
        clr = 1'b0;

        // 'a' = 0x1C make -> ascii 0x61
        do_clear; send_byte(8'h1C); repeat (4) @(posedge clk);
        check(n_ascii == 1 && last_ascii == 8'h61, "'a' -> 0x61");

        // F0 1C break -> no ascii
        do_clear; send_byte(8'hF0); send_byte(8'h1C); repeat (4) @(posedge clk);
        check(n_ascii == 0, "release ignored");

        // shift make (0x12) then 'a' -> 'A' (0x41)
        do_clear; send_byte(8'h12); send_byte(8'h1C); repeat (4) @(posedge clk);
        check(n_ascii == 1 && last_ascii == 8'h41, "shift+a -> 0x41");
        send_byte(8'hF0); send_byte(8'h12);           // shift release
        repeat (4) @(posedge clk);

        // E0 75 -> key_up
        do_clear; send_byte(8'hE0); send_byte(8'h75); repeat (4) @(posedge clk);
        check(n_up == 1 && n_ascii == 0, "E0 75 -> key_up");

        // E0 72 -> key_down
        do_clear; send_byte(8'hE0); send_byte(8'h72); repeat (4) @(posedge clk);
        check(n_down == 1, "E0 72 -> key_down");

        // 07 -> F12 -> key_toggle
        do_clear; send_byte(8'h07); repeat (4) @(posedge clk);
        check(n_toggle == 1, "07 (F12) -> key_toggle");

        // 5A -> enter
        do_clear; send_byte(8'h5A); repeat (4) @(posedge clk);
        check(n_enter == 1, "5A -> key_enter");

        // 76 -> esc
        do_clear; send_byte(8'h76); repeat (4) @(posedge clk);
        check(n_esc == 1, "76 -> key_esc");

        // 45 -> '0'
        do_clear; send_byte(8'h45); repeat (4) @(posedge clk);
        check(n_ascii == 1 && last_ascii == 8'h30, "45 -> '0'");

        #2000;
        if (errors == 0) $display("PS2 TB PASSED");
        else             $display("PS2 TB FAILED (%0d)", errors);
        $finish;
    end
endmodule
