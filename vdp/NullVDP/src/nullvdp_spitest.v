// ============================================================================
// nullvdp_spitest - minimal SPI-slave isolation build
//
// Instantiates ONLY the real spi_slave on the PMOD02 balls, so the z80neo can
// confirm whether the slave + wiring respond, independent of the full terminal
// top.  LEDs mirror the SPI activity.  Diagnostic build only.
// ============================================================================
module nullvdp_spitest (
    input        clk_27m,
    input        rst_n,
    input        spi_sck,
    input        spi_mosi,
    input        spi_cs_n,
    output       spi_miso,
    output [3:0] led
);
    wire [7:0] tx_byte;
    wire       tx_valid;
    wire       spi_active;

    // Always ready to consume console bytes so the TX FIFO never fills.
    wire tx_ready = 1'b1;

    spi_slave u_slave (
        .clk(clk_27m), .rst_n(rst_n),
        .spi_sck(spi_sck), .spi_mosi(spi_mosi), .spi_cs_n(spi_cs_n),
        .spi_miso(spi_miso),
        .tx_byte(tx_byte), .tx_valid(tx_valid), .tx_ready(tx_ready),
        .spi_active(spi_active),
        .rx_byte(8'h00), .rx_valid(1'b0)
    );

    assign led[0] = spi_active;   // pulses on every CS assertion
    assign led[1] = spi_sck;      // SCK as seen by the FPGA
    assign led[2] = spi_cs_n;     // CS as seen by the FPGA
    assign led[3] = spi_miso;     // MISO the slave drives
endmodule
