`timescale 1ns/1ps
module tb_menu;
    reg clk = 0, rst_n = 0;
    reg toggle = 0, up = 0, dn = 0, en = 0, esc = 0;
    wire active, box_open, ov_we;
    wire [9:0]  ov_addr;
    wire [7:0]  ov_char;
    wire [15:0] ov_attr;

    reg [7:0] disp [0:519];
    reg [15:0] dattr [0:519];

    always #18 clk = ~clk;      // ~27 MHz

    menu u_menu (
        .clk(clk), .rst_n(rst_n),
        .key_toggle(toggle), .key_up(up), .key_down(dn),
        .key_enter(en), .key_esc(esc),
        .active(active), .box_open(box_open),
        .ov_we(ov_we), .ov_addr(ov_addr), .ov_char(ov_char), .ov_attr(ov_attr)
    );

    always @(posedge clk) if (ov_we) begin disp[ov_addr] <= ov_char; dattr[ov_addr] <= ov_attr; end

    integer i, j;
    task dump; input [8*12:1] label;
        begin
            $display("=== %0s ===", label);
            for (i = 0; i < 13; i = i + 1) begin
                for (j = 0; j < 40; j = j + 1) $write("%c", disp[i*40+j]);
                if (i >= 3 && i <= 7) $write("   attr=%04x", dattr[i*40+2]);
                $write("\n");
            end
        end
    endtask

    initial begin
        repeat (4) @(posedge clk);
        rst_n = 1;
        repeat (4) @(posedge clk);

        toggle = 1; @(posedge clk); toggle = 0;
        repeat (1500) @(posedge clk);
        dump("MAIN MENU");

        dn = 1; @(posedge clk); dn = 0;
        repeat (1500) @(posedge clk);
        dump("AFTER DOWN");

        en = 1; @(posedge clk); en = 0;
        repeat (1500) @(posedge clk);
        dump("DISPLAY PAGE");

        $finish;
    end
endmodule
