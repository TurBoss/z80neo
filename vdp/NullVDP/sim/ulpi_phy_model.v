// ============================================================================
// ulpi_phy_model - behavioural USB3317 stand-in for simulation
//
//   * generates the 60 MHz ulpi_clk
//   * register read/write handshake (DIR/NXT/STP) per the ULPI spec
//   * packet TX accept (NXT per byte, commit on STP)
//
// The bus is all-Z whenever the link has no drive request, so "the link is
// driving" is exactly (ulpi_data !== 8'bz).
// ============================================================================
`timescale 1ns/1ps
module ulpi_phy_model (
    output reg        ulpi_clk,
    inout      [7:0]  ulpi_data,
    output reg        ulpi_dir,
    output reg        ulpi_nxt,
    input             ulpi_stp,
    // captured transmit packet (for testbenches)
    output reg [3:0]  tx_pid,
    output reg [4:0]  tx_len,
    output reg [95:0] tx_data,
    output reg        tx_stb
);
    initial begin
        ulpi_clk = 1'b0;
        ulpi_dir = 1'b0;
        ulpi_nxt = 1'b0;
    end
    always #8.33 ulpi_clk = ~ulpi_clk;      // ~60 MHz

    reg [7:0] regs [0:63];
    integer i;
    initial begin
        for (i = 0; i < 64; i = i + 1) regs[i] = 8'h00;
        regs[8'h00] = 8'h24;   // vendor ID low  (SMSC)
        regs[8'h01] = 8'h04;   // vendor ID high
        regs[8'h02] = 8'h07;   // product ID low
        regs[8'h03] = 8'h00;   // product ID high
        regs[8'h15] = 8'h02;   // Debug: LineState = 10 (low-speed device attached)
    end

    reg [7:0] dout;
    reg       drive;
    assign ulpi_data = drive ? dout : 8'bz;

    wire link_driving = (ulpi_data !== 8'bzzzzzzzz);

    localparam [2:0] P_IDLE = 0, P_RD_ACK = 1, P_RD_DATA = 2,
                     P_WR_ACK = 3, P_WR_DATA = 4, P_WR_STP = 5, P_TX = 6;
    reg [2:0] pst;
    reg [5:0] paddr;
    reg [7:0] ptmp;
    reg [1:0] pcnt;
    reg       tx_tog;
    reg [4:0] tx_cnt;
    reg [4:0] plen;
    reg [95:0] pbuf;
    reg [3:0]  txdly;

    always @(posedge ulpi_clk) begin
        ulpi_nxt <= 1'b0;
        case (pst)
        P_IDLE: begin
            ulpi_dir <= 1'b0;
            drive    <= 1'b0;
            tx_stb   <= 1'b0;
            if (!ulpi_stp && link_driving) begin
                case (ulpi_data[7:6])
                2'b11: begin paddr <= ulpi_data[5:0]; pst <= P_RD_ACK;  end
                2'b10: begin paddr <= ulpi_data[5:0]; pst <= P_WR_ACK;  end
                2'b01: begin
                           tx_pid <= ulpi_data[3:0];
                           pbuf   <= 96'd0;
                           tx_cnt <= 5'd0;
                           tx_tog <= 1'b0;
                           txdly  <= 4'd6;     // real PHY: NXT for TX CMD comes a few cycles late
                           pst    <= P_TX;
                       end
                default: ;
                endcase
            end
        end

        P_RD_ACK: begin
            ulpi_nxt <= 1'b1;                  // accept the command
            pcnt     <= 2'd0;
            pst      <= P_RD_DATA;
        end

        P_RD_DATA: begin
            ulpi_dir <= 1'b1;
            ulpi_nxt <= 1'b1;
            drive    <= 1'b1;
            dout     <= regs[paddr];
            if (pcnt == 2'd2) begin
                ulpi_dir <= 1'b0;
                drive    <= 1'b0;
                pst      <= P_IDLE;
            end else pcnt <= pcnt + 2'd1;
        end

        P_WR_ACK: begin
            ulpi_nxt <= 1'b1;                  // accept the command
            pst      <= P_WR_DATA;
        end

        P_WR_DATA: begin
            ulpi_nxt <= 1'b1;                  // accept the data byte
            ptmp     <= ulpi_data;
            pst      <= P_WR_STP;
        end

        P_WR_STP: begin
            if (ulpi_stp) begin
                regs[paddr] <= ptmp;
                pst <= P_IDLE;
            end
        end

        P_TX: begin
            if (txdly != 0) txdly <= txdly - 4'd1;
            else tx_tog <= ~tx_tog;
            if (txdly == 0 && tx_tog) begin
                ulpi_nxt <= 1'b1;              // accept one byte on alternate cycles
                pbuf[tx_cnt*8 +: 8] <= ulpi_data;
                tx_cnt <= tx_cnt + 5'd1;
            end
            if (ulpi_stp) begin
                tx_data <= pbuf;
                tx_len  <= tx_cnt;
                tx_stb  <= 1'b1;
                pst     <= P_IDLE;
            end
        end

        default: pst <= P_IDLE;
        endcase
    end
endmodule
