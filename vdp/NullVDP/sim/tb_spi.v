// ============================================================================
// tb_spi - framed SPI link test
// ============================================================================
`timescale 1ns/1ps

module tb_spi;
    reg clk = 0, rst_n = 0;
    always #18.5 clk = ~clk;

    reg sck = 0, mosi = 0, cs_n = 1;
    wire miso;
    wire [7:0] tx_byte;
    wire tx_valid;
    reg tx_ready = 0;
    reg [7:0] rx_byte = 0;
    reg rx_valid = 0;

    spi_slave dut (
        .clk(clk), .rst_n(rst_n), .spi_sck(sck), .spi_mosi(mosi), .spi_cs_n(cs_n),
        .spi_miso(miso), .tx_byte(tx_byte), .tx_valid(tx_valid), .tx_ready(tx_ready),
        .rx_byte(rx_byte), .rx_valid(rx_valid)
    );

    integer errors = 0;
    reg [31:0] rx_in;

    function [7:0] crc8_24;
        input [23:0] value;
        integer byte, bit;
        reg [7:0] crc, data;
        begin
            crc = 0;
            for (byte = 2; byte >= 0; byte = byte - 1) begin
                data = value >> (byte * 8);
                crc = crc ^ data;
                for (bit = 0; bit < 8; bit = bit + 1)
                    crc = crc[7] ? ((crc << 1) ^ 8'h07) : (crc << 1);
            end
            crc8_24 = crc;
        end
    endfunction

    function [31:0] frame;
        input [7:0] kind, sequence, data;
        reg [23:0] payload;
        begin
            payload = {kind, sequence, data};
            frame = {payload, crc8_24(payload)};
        end
    endfunction

    task spi_xfer;
        input [31:0] out;
        output [31:0] response;
        integer i;
        begin
            cs_n = 0; #200;
            rx_in = 0;
            for (i = 31; i >= 0; i = i - 1) begin
                mosi = out[i];
                #60; sck = 1; #120;
                rx_in = {rx_in[30:0], miso};
                sck = 0; #60;
            end
            #100; cs_n = 1; #200;
            response = rx_in;
        end
    endtask

    reg [31:0] response;
    initial begin
        repeat (10) @(posedge clk);
        rst_n = 1;
        repeat (10) @(posedge clk);

        // Synchronize sequence state, then poll returns the ID.
        spi_xfer(frame(8'h5B, 0, 0), response);
        spi_xfer(frame(8'h5A, 0, 0), response);
        if (response[15:8] !== 8'h4E)
            begin $display("FAIL id got %02x", response[15:8]); errors = errors + 1; end
        else $display("PASS id = %02x", response[15:8]);

        // Send one byte, then poll to observe its acknowledgement.
        spi_xfer(frame(8'hA5, 0, 8'h41), response);
        spi_xfer(frame(8'h5A, 0, 0), response);
        if (!tx_valid || tx_byte !== 8'h41)
            begin $display("FAIL tx handoff valid=%b byte=%02x", tx_valid, tx_byte); errors = errors + 1; end
        else $display("PASS framed console byte 'A'");

        @(posedge clk); tx_ready = 1;
        @(posedge clk); tx_ready = 0;
        #200;
        if (tx_valid) begin $display("FAIL tx not consumed"); errors = errors + 1; end
        else $display("PASS console byte consumed");

        // A duplicate frame must be acknowledged without duplicating the byte.
        spi_xfer(frame(8'hA5, 0, 8'h41), response);
        spi_xfer(frame(8'h5A, 0, 0), response);
        if (tx_valid) begin $display("FAIL duplicate delivered"); errors = errors + 1; end
        else $display("PASS duplicate suppressed");

        // Keyboard byte is returned in the poll response after one pipeline turn.
        @(posedge clk); rx_byte = 8'h5A; rx_valid = 1;
        @(posedge clk); rx_valid = 0;
        spi_xfer(frame(8'h5A, 0, 0), response);
        spi_xfer(frame(8'h5A, 0, 0), response);
        if (response[15:8] !== 8'h5A)
            begin $display("FAIL RX read got %02x", response[15:8]); errors = errors + 1; end
        else $display("PASS RX read = %02x", response[15:8]);

        $display("SPI TB %s", errors == 0 ? "PASSED" : "FAILED");
        $finish;
    end
endmodule
