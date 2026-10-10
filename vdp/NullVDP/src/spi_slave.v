// ============================================================================
// spi_slave - NullVDP <-> z80neo SPI link (slave side)
//
// The Pico (z80neo) is the SPI master; this is the NullVDP slave.  Mode 0
// (CPOL=0, CPHA=0), MSB-first, active-low CS.  SCK is oversampled in the
// 27 MHz clk domain, so keep SCK at or below ~6 MHz.
//
// Each transaction is 16 bits:
//   master -> slave : CMD[7:0], DATA[7:0]
//   slave  -> master: STATUS[7:0], RDATA[7:0]
// CMD = (addr[6:0] << 1) | rw  (rw=1 write, 0 read).
//
// Registers (addr):
//   0x00 STATUS  R  bit0 TX_READY, bit1 RX_AVAIL, bit2 RX_OVF, bit7=1
//   0x01 TX_DATA W  console byte to display
//   0x02 RX_DATA R  keyboard byte to the host
//   0x03 CTRL    W  write 1 clears RX_OVF
//   0x04 ID      R  0x4E ('N')
//
// MISO changes on the SCK falling edge (byte 0 = STATUS, byte 1 = RDATA), so
// it is stable at each rising edge the master samples it.  STATUS is returned
// in every transaction, so the master learns TX_READY / RX_AVAIL from the
// previous transfer.
// ============================================================================
module spi_slave (
    input             clk,
    input             rst_n,

    // SPI (from the Pico master)
    input             spi_sck,
    input             spi_mosi,
    input             spi_cs_n,
    output            spi_miso,

    // console bytes to the terminal (host -> display)
    output     [7:0]  tx_byte,
    output            tx_valid,
    input             tx_ready,     // terminal_handler rx_ready

    // one-cycle pulse on every transaction: proves the host is present even
    // when it has no console byte to send (keepalive)
    output reg        spi_active,

    // keyboard bytes to the host (local key -> host)
    input      [7:0]  rx_byte,
    input             rx_valid
);
    localparam [6:0] REG_STATUS = 7'h00,
                     REG_TX     = 7'h01,
                     REG_RX     = 7'h02,
                     REG_CTRL   = 7'h03,
                     REG_ID     = 7'h04;

    localparam [7:0] ID_VALUE = 8'h4E;   // 'N'

    // ---------------- input synchronisers ----------------
    reg [2:0] sck_s, cs_s;
    reg [1:0] mosi_s;
    always @(posedge clk) begin
        sck_s  <= {sck_s[1:0],  spi_sck};
        cs_s   <= {cs_s[1:0],   spi_cs_n};
        mosi_s <= {mosi_s[0],   spi_mosi};
    end
    wire sck_rise = (sck_s[2:1] == 2'b01);
    wire sck_fall = (sck_s[2:1] == 2'b10);
    wire cs_fall  = (cs_s[2:1]  == 2'b10);
    wire cs_rise  = (cs_s[2:1]  == 2'b01);
    wire cs_low   = ~cs_s[1];
    wire mosi     = mosi_s[1];

    // ---------------- FIFOs ----------------
    reg  [7:0] txf [0:7];
    reg  [3:0] tx_wp, tx_rp, tx_cnt;
    wire       tx_full  = (tx_cnt == 4'd8);
    wire       tx_empty = (tx_cnt == 4'd0);

    reg  [7:0] rxf [0:7];
    reg  [3:0] rx_wp, rx_rp, rx_cnt;
    reg        rx_ovf;
    wire       rx_full  = (rx_cnt == 4'd8);
    wire       rx_avail = (rx_cnt != 4'd0);

    assign tx_byte  = txf[tx_rp[2:0]];
    assign tx_valid = !tx_empty;
    wire   tx_pop   = !tx_empty && tx_ready;

    // ---------------- transaction engine ----------------
    reg  [3:0] bitcnt;     // bits received (0..16)
    reg  [7:0] sr_in;      // received CMD/DATA
    reg  [7:0] sr_out;     // byte currently shifting out
    reg  [6:0] cmd;
    reg        cmd_rw;

    wire [7:0] received = {sr_in[6:0], mosi};
    wire       rx_pop   = cs_rise && !cmd_rw && (cmd == REG_RX) && rx_avail;

    wire [7:0] status_reg = {1'b1, 3'b000, 1'b0, rx_ovf, rx_avail, !tx_full};

    reg [7:0] rd_byte;
    always @(*) begin
        case (cmd)
            REG_STATUS: rd_byte = status_reg;
            REG_RX:     rd_byte = rxf[rx_rp[2:0]];
            REG_ID:     rd_byte = ID_VALUE;
            default:    rd_byte = 8'h00;
        endcase
    end

    assign spi_miso = sr_out[7];

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            tx_wp <= 4'd0; tx_rp <= 4'd0; tx_cnt <= 4'd0;
            rx_wp <= 4'd0; rx_rp <= 4'd0; rx_cnt <= 4'd0; rx_ovf <= 1'b0;
            bitcnt <= 4'd0; sr_in <= 8'd0; sr_out <= 8'd0;
            cmd <= 7'd0; cmd_rw <= 1'b0; spi_active <= 1'b0;
        end else begin
            spi_active <= cs_fall;      // pulse on every transaction (keepalive)

            // ---- CS edge ----
            if (cs_fall) begin
                bitcnt <= 4'd0;
                sr_in  <= 8'd0;
                sr_out <= status_reg;          // byte 0 starts as soon as CS asserts
            end
            if (cs_rise && cmd_rw && (cmd == REG_CTRL))
                rx_ovf <= 1'b0;

            // ---- SCK edges ----
            if (cs_low && sck_rise) begin
                sr_in  <= received;
                bitcnt <= bitcnt + 4'd1;
                if (bitcnt == 4'd7) begin
                    cmd    <= received[7:1];
                    cmd_rw <= received[0];
                end
            end else if (cs_low && sck_fall) begin
                if (bitcnt == 4'd8) sr_out <= rd_byte;         // switch to byte 1
                else                sr_out <= {sr_out[6:0], 1'b0};
            end

            // ---- TX FIFO push (write to REG_TX) ----
            if (cs_low && sck_rise && bitcnt == 4'd15 &&
                cmd_rw && (cmd == REG_TX) && !tx_full) begin
                txf[tx_wp[2:0]] <= received;
                tx_wp  <= tx_wp + 4'd1;
                tx_cnt <= tx_cnt + 4'd1;
            end

            // ---- TX FIFO pop by the terminal ----
            if (tx_pop) begin
                tx_rp  <= tx_rp + 4'd1;
                tx_cnt <= tx_cnt - 4'd1;
            end

            // ---- RX FIFO push from the keyboard ----
            if (rx_valid && !rx_full) begin
                rxf[rx_wp[2:0]] <= rx_byte;
                rx_wp  <= rx_wp + 4'd1;
                rx_cnt <= rx_cnt + 4'd1;
            end else if (rx_valid && rx_full) begin
                rx_ovf <= 1'b1;
            end

            // ---- RX FIFO pop on a completed read of REG_RX ----
            if (rx_pop) begin
                rx_rp  <= rx_rp + 4'd1;
                rx_cnt <= rx_cnt - 4'd1;
            end
        end
    end
endmodule
