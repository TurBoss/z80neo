// ============================================================================
// tb_ulpi_reg - verify ulpi_link register read AND write against a PHY model.
// This is the check the flaky bench kept obscuring: both must work in one
// build, and reads must still work after a write.
// ============================================================================
`timescale 1ns/1ps
module tb_ulpi_reg;
    wire       ulpi_clk;
    wire [7:0] ulpi_data;
    wire       ulpi_dir, ulpi_nxt, ulpi_stp;

    ulpi_phy_model phy (
        .ulpi_clk(ulpi_clk), .ulpi_data(ulpi_data),
        .ulpi_dir(ulpi_dir), .ulpi_nxt(ulpi_nxt), .ulpi_stp(ulpi_stp)
    );

    wire ulclk = ~ulpi_clk;      // this board's ULPI clock is inverted
    reg  rst_n = 0;

    reg        r_start = 0;   // toggle-handshake request
    reg        r_wr = 0;
    reg  [5:0] r_addr = 0;
    reg  [7:0] r_wdata = 0;
    wire [7:0] r_rdata;
    wire       r_done;

    ulpi_link link (
        .clk(ulclk), .rst_n(rst_n),
        .ulpi_data(ulpi_data), .ulpi_dir(ulpi_dir), .ulpi_nxt(ulpi_nxt),
        .ulpi_stp(ulpi_stp),
            .r_req(r_start), .r_wr(r_wr), .r_addr(r_addr), .r_wdata(r_wdata),
        .r_rdata(r_rdata), .r_done(r_done), .r_busy(),
        .tx_req(1'b0), .tx_pid(4'd0), .tx_len(4'd0), .tx_data(64'd0),
        .tx_busy(), .tx_done(),
        .rx_pid(), .rx_len(), .rx_data(), .rx_valid(), .rx_error(),
        .line_state(), .dbg_state()
    );

    integer errors = 0;

    task do_write(input [5:0] a, input [7:0] d);
        integer t;
        begin
            @(posedge ulclk);
            r_addr = a; r_wdata = d; r_wr = 1'b1; r_start = ~r_start;
            t = 0;
            while (!r_done && t < 300) begin @(posedge ulclk); t = t + 1; end
            if (!r_done) begin $display("FAIL: write %02x timeout", a); errors = errors + 1; end
            @(posedge ulclk);
        end
    endtask

    task do_read(input [5:0] a, output [7:0] d);
        integer t;
        begin
            @(posedge ulclk);
            r_addr = a; r_wr = 1'b0; r_start = ~r_start;
            t = 0;
            while (!r_done && t < 300) begin @(posedge ulclk); t = t + 1; end
            if (!r_done) begin $display("FAIL: read %02x timeout", a); errors = errors + 1; d = 8'hxx; end
            else d = r_rdata;
            @(posedge ulclk);
        end
    endtask

    reg [7:0] v;
    task check_read(input [5:0] a, input [7:0] exp, input [255:0] msg);
        begin
            do_read(a, v);
            if (v !== exp) begin
                $display("FAIL: %0s: reg[%02x]=%02x expect %02x", msg, a, v, exp);
                errors = errors + 1;
            end else
                $display("ok:   %0s: reg[%02x]=%02x", msg, a, v);
        end
    endtask

    initial begin
        repeat (20) @(posedge ulclk);
        rst_n = 1;
        repeat (20) @(posedge ulclk);

        check_read(6'h00, 8'h24, "vendor ID low");

        do_write(6'h04, 8'h45);
        check_read(6'h04, 8'h45, "function control write/read");

        do_write(6'h06, 8'h26);
        check_read(6'h06, 8'h26, "OTG control write/read");

        check_read(6'h00, 8'h24, "vendor ID low after writes");

        if (errors == 0) $display("ULPI REG TB PASSED");
        else             $display("ULPI REG TB FAILED (%0d)", errors);
        $finish;
    end
endmodule
