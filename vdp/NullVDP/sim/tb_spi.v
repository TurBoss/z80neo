// ============================================================================
// tb_spi - SPI slave register/link test
//
// Models the Pico as an SPI mode-0 master and checks:
//   - write TX_DATA -> tx_valid/tx_byte to the terminal
//   - TX_READY back-pressure (status bit0)
//   - keyboard push -> RX_AVAIL (status bit1) + RX_DATA read
//   - ID register
// ============================================================================
`timescale 1ns/1ps

module tb_spi;
    reg clk = 0, rst_n = 0;
    always #18.5 clk = ~clk;          // ~27 MHz

    reg        sck = 0, mosi = 0, cs_n = 1;
    wire       miso;
    wire [7:0] tx_byte;
    wire       tx_valid;
    reg        tx_ready = 0;
    reg  [7:0] rx_byte = 0;
    reg        rx_valid = 0;

    spi_slave dut (
        .clk(clk), .rst_n(rst_n),
        .spi_sck(sck), .spi_mosi(mosi), .spi_cs_n(cs_n), .spi_miso(miso),
        .tx_byte(tx_byte), .tx_valid(tx_valid), .tx_ready(tx_ready),
        .rx_byte(rx_byte), .rx_valid(rx_valid)
    );

    integer errors = 0;
    reg [15:0] rx_in;

    // 16-bit mode-0 transfer: send {cmd,data}, return {status,rdata}
    task spi_xfer;
        input  [7:0] cmd, data;
        output [7:0] status, rdata;
        integer i;
        reg [15:0] out;
        begin
            cs_n = 0;
            #200;
            out = {cmd, data};
            rx_in = 0;
            for (i = 15; i >= 0; i = i - 1) begin
                mosi = out[i];
                #60;
                sck = 1;
                #120;
                rx_in = {rx_in[14:0], miso};
                sck = 0;
                #60;
            end
            #100;
            cs_n = 1;
            #200;
            status = rx_in[15:8];
            rdata  = rx_in[7:0];
        end
    endtask

    reg [7:0] st, rd;

    initial begin
        repeat (10) @(posedge clk);
        rst_n = 1;
        repeat (10) @(posedge clk);

        // --- ID register ---
        spi_xfer(8'h08, 8'h00, st, rd);        // read addr 0x04
        if (rd !== 8'h4E) begin $display("FAIL id got %02x", rd); errors = errors + 1; end
        else $display("PASS id = %02x", rd);

        // --- STATUS: expect bit7=1, TX_READY=1, RX_AVAIL=0 ---
        spi_xfer(8'h00, 8'h00, st, rd);
        if (st[7] !== 1'b1 || st[0] !== 1'b1 || st[1] !== 1'b0) begin
            $display("FAIL status init %02x", st); errors = errors + 1;
        end else $display("PASS status init = %02x", st);

        // --- write a console byte (hold tx_ready so we can observe the handoff)
        tx_ready = 0;
        spi_xfer(8'h03, 8'h41, st, rd);        // write addr 0x01, 'A'
        #200;
        if (!tx_valid || tx_byte !== 8'h41) begin
            $display("FAIL tx handoff valid=%b byte=%02x", tx_valid, tx_byte);
            errors = errors + 1;
        end else $display("PASS console byte 'A' handed to terminal");
        // terminal consumes it
        @(posedge clk); tx_ready = 1;
        @(posedge clk); tx_ready = 0;
        #200;
        if (tx_valid) begin $display("FAIL tx not consumed"); errors = errors + 1; end
        else $display("PASS console byte consumed");

        // --- keyboard push, then read RX_DATA ---
        @(posedge clk); rx_byte = 8'h5A; rx_valid = 1;
        @(posedge clk); rx_valid = 0;
        #200;
        spi_xfer(8'h00, 8'h00, st, rd);        // status: RX_AVAIL
        if (st[1] !== 1'b1) begin $display("FAIL RX_AVAIL not set %02x", st); errors = errors + 1; end
        else $display("PASS RX_AVAIL set");
        spi_xfer(8'h04, 8'h00, st, rd);        // read addr 0x02
        if (rd !== 8'h5A) begin $display("FAIL RX read got %02x", rd); errors = errors + 1; end
        else $display("PASS RX read = %02x", rd);
        #200;
        if (tx_valid) begin $display("INFO tx still valid unexpectedly"); end

        $display("SPI TB %s", errors == 0 ? "PASSED" : "FAILED");
        $finish;
    end
endmodule
