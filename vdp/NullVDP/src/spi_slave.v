// ============================================================================
// spi_slave - reliable 32-bit framed SPI link to the z80neo Pico
//
// Host -> slave frames:
//   A5 sequence byte data byte CRC8
//   5A 00 00 CRC8                 poll / keyboard read
// Slave -> host frames:
//   status ack-sequence rx-byte CRC8
//
// CRC-8 uses polynomial 0x07 over the first three bytes.  A TX frame is
// accepted only when its CRC and sequence are valid.  Repeating the last frame
// after a lost reply is harmless: the duplicate is acknowledged but not
// delivered to the terminal twice.
// ============================================================================
module spi_slave (
    input             clk,
    input             rst_n,
    input             spi_sck,
    input             spi_mosi,
    input             spi_cs_n,
    output            spi_miso,
    output     [7:0]  tx_byte,
    output            tx_valid,
    input             tx_ready,
    output reg        spi_active,
    input      [7:0]  rx_byte,
    input             rx_valid
);
    localparam [7:0] FRAME_TX   = 8'hA5;
    localparam [7:0] FRAME_POLL = 8'h5A;
    localparam [7:0] FRAME_SYNC = 8'h5B;
    localparam [7:0] ID_VALUE   = 8'h4E;

    reg [2:0] sck_s, cs_s;
    reg [1:0] mosi_s;
    always @(posedge clk) begin
        sck_s  <= {sck_s[1:0], spi_sck};
        cs_s   <= {cs_s[1:0], spi_cs_n};
        mosi_s <= {mosi_s[0], spi_mosi};
    end
    wire sck_rise = (sck_s[2:1] == 2'b01);
    wire sck_fall = (sck_s[2:1] == 2'b10);
    wire cs_fall  = (cs_s[2:1] == 2'b10);
    wire cs_rise  = (cs_s[2:1] == 2'b01);
    wire cs_low   = ~cs_s[1];
    wire mosi     = mosi_s[1];

    reg [7:0] txf [0:7];
    reg [3:0] tx_wp, tx_rp, tx_cnt;
    wire tx_full  = (tx_cnt == 4'd8);
    wire tx_empty = (tx_cnt == 4'd0);
    assign tx_byte = txf[tx_rp[2:0]];
    assign tx_valid = !tx_empty;

    reg [7:0] rxf [0:7];
    reg [3:0] rx_wp, rx_rp, rx_cnt;
    reg       rx_ovf;
    wire rx_full  = (rx_cnt == 4'd8);
    wire rx_avail = (rx_cnt != 4'd0);

    reg [5:0] bitcnt;
    reg [31:0] sr_in, sr_out;
    reg [31:0] response_frame;
    reg [7:0] ack_seq;

    function [7:0] crc8_24;
        input [23:0] value;
        integer byte, bit;
        reg [7:0] crc;
        reg [7:0] data;
        begin
            crc = 8'd0;
            for (byte = 2; byte >= 0; byte = byte - 1) begin
                data = value >> (byte * 8);
                crc = crc ^ data;
                for (bit = 0; bit < 8; bit = bit + 1)
                    crc = crc[7] ? ((crc << 1) ^ 8'h07) : (crc << 1);
            end
            crc8_24 = crc;
        end
    endfunction

    wire frame_crc_ok = (crc8_24(sr_in[31:8]) == sr_in[7:0]);
    wire frame_tx = (sr_in[31:24] == FRAME_TX);
    wire frame_poll = (sr_in[31:24] == FRAME_POLL);
    wire frame_sync = (sr_in[31:24] == FRAME_SYNC);
    wire tx_pop = !tx_empty && tx_ready && !frame_sync;
    wire seq_expected = (sr_in[23:16] == (ack_seq + 8'd1));
    wire seq_duplicate = (sr_in[23:16] == ack_seq);
    wire tx_frame_valid = cs_rise && frame_tx && frame_crc_ok;
    wire tx_new = tx_frame_valid && seq_expected && !tx_full;
    wire rx_pop = cs_rise && frame_poll && rx_avail;
    wire rx_push = rx_valid && !rx_full;
    wire [7:0] status_reg = {1'b1, 3'b000, 1'b0, rx_ovf, rx_avail, !tx_full};
    wire [7:0] response_ack = frame_sync ? 8'hFF :
                               (tx_new ? sr_in[23:16] : ack_seq);
    wire [7:0] response_data = (frame_sync || (response_ack == 8'hFF && !rx_avail)) ? ID_VALUE :
                                (rx_avail ? rxf[rx_rp[2:0]] : 8'h00);

    assign spi_miso = sr_out[31];

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            tx_wp <= 4'd0; tx_rp <= 4'd0; tx_cnt <= 4'd0;
            rx_wp <= 4'd0; rx_rp <= 4'd0; rx_cnt <= 4'd0; rx_ovf <= 1'b0;
            bitcnt <= 6'd0; sr_in <= 32'd0; sr_out <= 32'd0;
            response_frame <= {8'h81, 8'hFF, ID_VALUE, crc8_24(24'h81FF4E)};
            ack_seq <= 8'hFF;
            spi_active <= 1'b0;
        end else begin
            spi_active <= cs_fall;

            if (cs_fall) begin
                bitcnt <= 6'd0;
                sr_in <= 32'd0;
                sr_out <= response_frame;
            end

            if (cs_low && sck_rise) begin
                sr_in <= {sr_in[30:0], mosi};
                bitcnt <= bitcnt + 6'd1;
            end else if (cs_low && sck_fall) begin
                sr_out <= {sr_out[30:0], 1'b0};
            end

            if (frame_sync) begin
                ack_seq <= 8'hFF;
                tx_wp <= 4'd0;
                tx_rp <= 4'd0;
                tx_cnt <= 4'd0;
            end else if (tx_new)
                ack_seq <= sr_in[23:16];

            if (tx_new)
                txf[tx_wp[2:0]] <= sr_in[15:8];
            if (tx_new != tx_pop)
                tx_cnt <= tx_new ? (tx_cnt + 4'd1) : (tx_cnt - 4'd1);
            if (tx_new) tx_wp <= tx_wp + 4'd1;
            if (tx_pop) tx_rp <= tx_rp + 4'd1;

            if (rx_push) rxf[rx_wp[2:0]] <= rx_byte;
            if (rx_valid && rx_full) rx_ovf <= 1'b1;
            if (rx_push != rx_pop)
                rx_cnt <= rx_push ? (rx_cnt + 4'd1) : (rx_cnt - 4'd1);
            if (rx_push) rx_wp <= rx_wp + 4'd1;
            if (rx_pop) rx_rp <= rx_rp + 4'd1;
            if (cs_rise && frame_tx && sr_in[31:24] == 8'h03 && sr_in[15:8] == 8'h01)
                rx_ovf <= 1'b0;

            // Prepare the response for the next transaction.  The current
            // transaction has already shifted the previous response out.
            if (cs_rise && frame_crc_ok && (frame_tx || frame_poll || frame_sync)) begin
                response_frame <= {
                    status_reg,
                    response_ack,
                    response_data,
                    crc8_24({status_reg,
                             response_ack,
                             response_data})
                };
            end
        end
    end
endmodule
