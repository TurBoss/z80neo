// ============================================================================
// ULPI link: register window + USB packet TX + packet RX, in ONE module.
// ============================================================================
//
// A single module owns the ULPI data pad; the register FSM and the packet TX
// FSM share one clocked block driving out_req/data_out -- the idiom that reads
// this board's pad correctly (splitting the pad across modules loses the input
// path on this toolchain).
//
//   register : command 0xC0|addr = read, 0x80|addr = write
//   transmit : command 0x40|PID[3:0], then payload bytes, STP = end of packet
//   receive  : DIR=1 & NXT=1 -> packet byte; DIR=1 & NXT=0 -> status (RXCMD)
//
// The register FSM is a direct port of the Sipeed/Luna `register_window`
// state machine (known-good against the USB3317 device stack), so its NXT/DIR
// turnaround timing matches the reference.
//
// All logic is synchronous to ~ulpi_clk (this board's ULPI clock is inverted).
// ============================================================================

module ulpi_link #(parameter [3:0] TRIG_PID = 4'hD,
                   parameter       FLUSH    = 1) (
    input            clk,
    input            rst_n,
    inout      [7:0] ulpi_data,
    input            ulpi_dir,
    input            ulpi_nxt,
    output           ulpi_stp,

    // register window
    input            r_req,        // toggled by the link's client to start an op
    input            r_wr,
    input      [5:0] r_addr,
    input      [7:0] r_wdata,
    output reg [7:0] r_rdata,
    output reg       r_done,
    output           r_busy,

    // packet transmit
    input            tx_req,
    input      [3:0] tx_pid,
    input      [3:0] tx_len,
    input      [95:0] tx_data,
    output reg       tx_busy,
    output reg       tx_done,

    // packet receive
    output reg [7:0] rx_pid,
    output reg [4:0] rx_len,
    output reg [255:0] rx_data,
    output reg       rx_valid,
    output reg       rx_error,
    output     [1:0] line_state,
    output     [7:0] rxcmd_raw,
    output reg [7:0] rxcmd_cnt,
    output reg [7:0] tx_abort_cnt,
    output reg [7:0] tx_dir_cnt,
    output reg [7:0] rxstart_cnt,    // turnaround with NXT=1 (USB receive starts)
    output reg [7:0] rxbyte_cnt,     // received packet bytes
    output reg [7:0] rxactcmd_cnt,   // RX CMDs with RxActive set
    input            ls_mode,        // 1 = low-speed bus timing

    // cycle trace from the first SETUP transmit: {dir,nxt,stp,oe,data}
    input      [9:0] trace_addr,
    output reg [31:0] trace_q,
    output reg       trace_done,

    output     [3:0] dbg_state,
    output reg [7:0] dbg_tx_byte,
    output reg       dbg_tx_stb
);

    // ---------------- register FSM (Sipeed/Luna register_window) -----------
    localparam [3:0]
        R_IDLE    = 4'd0,
        R_CMD_RD  = 4'd1,   // drive 0xC0|addr
        R_CMD_WR  = 4'd2,   // drive 0x80|addr
        R_RD_ACK  = 4'd3,   // wait NXT (DIR must stay low)
        R_RD_WAIT = 4'd4,
        R_RD_DONE = 4'd5,   // sample data
        R_WR_ACK  = 4'd6,   // wait NXT for command
        R_WR_DATA = 4'd7,   // wait NXT for data, then STP
        R_WR_DONE = 4'd8;

    // ---------------- packet TX FSM ----------------------------------------
    localparam [3:0]
        T_IDLE = 4'd0, T_CMD = 4'd1, T_DATA = 4'd2, T_STP = 4'd3, T_DONE = 4'd4;

    reg [3:0] rstate;
    reg [3:0] tstate;
    reg [5:0] caddr;
    reg [7:0] cwrite;
    reg [3:0] tidx;
    reg       pending;
    reg [7:0]  rxcmd;
    reg        rxcmd_stb;
    reg        dir_d;
    reg [11:0] tstall;   // > one low-speed byte time (5.3 us) between NXTs
    reg        eop_wait, eop_se0;
    reg [13:0] eop_to;
    reg       r_req_d;
    reg       hreq;        // latched host register request
    reg       flush_ok;    // scratch read done for the current transaction
    reg       flush_act;   // current register op is the internal flush
    reg [11:0] idle_cnt;   // cycles the link has been idle

    reg [7:0] data_out;
    wire      bus_free;
    wire      dir_d_o;
    reg       out_req;
    reg       ulpi_stp_r;

    // ULPI: the link must drive the bus whenever DIR is low (idle = 00h); a
    // floating bus is decoded by the PHY as random TX/register commands.
    assign ulpi_data = (ulpi_dir | dir_d_o) ? 8'bz : (out_req ? data_out : 8'h00);
    assign ulpi_stp  = ulpi_stp_r;
    assign r_busy    = (rstate != R_IDLE);
    assign dbg_state = rstate;

    function [7:0] tbyte;
        input [3:0] i;
        begin
            tbyte = tx_data[i*8 +: 8];
        end
    endfunction

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            rstate    <= R_IDLE;
            tstate    <= T_IDLE;
            caddr     <= 6'd0;
            cwrite    <= 8'd0;
            tidx      <= 4'd0;
            pending   <= 1'b0;
            tstall    <= 12'd0;
            eop_wait  <= 1'b0; eop_se0 <= 1'b0; eop_to <= 14'd0;
            r_req_d   <= 1'b0;
            hreq      <= 1'b0; flush_ok <= 1'b1; flush_act <= 1'b0; idle_cnt <= 12'd0;
            data_out  <= 8'h00;
            out_req   <= 1'b0;
            ulpi_stp_r <= 1'b0;
            r_rdata   <= 8'h00;
            r_done    <= 1'b0;
            tx_busy   <= 1'b0;
            tx_done   <= 1'b0;
            dbg_tx_stb <= 1'b0;
            tx_abort_cnt <= 8'd0;
            tx_dir_cnt <= 8'd0;
        end else begin
            if ((tstate == T_CMD || tstate == T_DATA) && ulpi_dir) tx_dir_cnt <= tx_dir_cnt + 8'd1;
            r_done  <= 1'b0;
            dbg_tx_stb <= 1'b0;
            tx_done <= 1'b0;
            r_req_d <= r_req;

            // ---------------- register FSM (runs when TX idle) ------------
            case (rstate)
            R_IDLE: begin
                if (tstate == T_IDLE) begin     // don't release a packet the TX FSM is holding
                    out_req    <= 1'b0;
                    ulpi_stp_r <= 1'b0;
                end
                // USB3317: a packet that was fully buffered before STP leaves a
                // copy of its last byte in the PHY TX buffer, which is then sent
                // in front of the next packet's PID.  One register access before
                // the first packet of a transaction clears it; back-to-back
                // packets inside a transaction (token->DATA) are unaffected.
                if (tstate == T_IDLE && hreq) begin
                    hreq   <= 1'b0;
                    caddr  <= r_addr;
                    cwrite <= r_wdata;
                    rstate <= r_wr ? R_CMD_WR : R_CMD_RD;
                end else if (tstate == T_IDLE && !flush_ok && (tx_req || pending) &&
                             bus_free && !eop_wait) begin
                    caddr     <= 6'h16;                // scratch register
                    flush_act <= 1'b1;
                    rstate    <= R_CMD_RD;
                end
            end

            R_CMD_RD: if (bus_free) begin
                data_out <= 8'hC0 | caddr;
                out_req  <= 1'b1;
                rstate   <= R_RD_ACK;
            end

            R_RD_ACK: begin
                out_req <= 1'b1;
                if (ulpi_dir) begin
                    out_req <= 1'b0;
                    rstate  <= R_CMD_RD;               // bus busy: retry
                end else if (ulpi_nxt) begin
                    out_req  <= 1'b0;
                    data_out <= 8'h00;
                    rstate   <= R_RD_WAIT;
                end
            end

            R_RD_WAIT: if (ulpi_dir) rstate <= R_RD_DONE;   // wait for the PHY to drive

            R_RD_DONE: begin
                if (!flush_act) begin
                    r_rdata <= ulpi_data;             // sampled while DIR is high
                    r_done  <= 1'b1;
                end else begin
                    flush_ok  <= 1'b1;                // buffer clear for this txn
                end
                flush_act <= 1'b0;
                rstate    <= R_IDLE;
            end

            R_CMD_WR: if (bus_free) begin
                data_out <= 8'h80 | caddr;
                out_req  <= 1'b1;
                rstate   <= R_WR_ACK;
            end

            R_WR_ACK: begin
                out_req <= 1'b1;
                if (ulpi_dir) begin
                    out_req <= 1'b0;
                    rstate  <= R_CMD_WR;               // bus busy: retry
                end else if (ulpi_nxt) begin
                    data_out <= cwrite;
                    rstate   <= R_WR_DATA;
                end
            end

            R_WR_DATA: begin
                out_req <= 1'b1;
                if (ulpi_dir) begin
                    out_req <= 1'b0;
                    rstate  <= R_CMD_WR;               // bus busy: retry
                end else if (ulpi_nxt) begin
                    out_req    <= 1'b0;
                    data_out   <= 8'h00;
                    ulpi_stp_r <= 1'b1;
                    rstate     <= R_WR_DONE;
                end
            end

            R_WR_DONE: begin
                ulpi_stp_r <= 1'b0;
                r_done     <= 1'b1;
                rstate     <= R_IDLE;
            end

            default: rstate <= R_IDLE;
            endcase

            // ---------------- inter-packet gap ----------------
            // The PHY buffers the next packet before the previous one has left
            // the wire, so the link must hold off until this packet's EOP (the
            // PHY reports its own line activity: SE0 then idle) -- USB needs
            // idle between packets (token -> DATA).  The USB3317 reports its EOP
            // only after the whole USB transaction, far too late to gate on: the
            // device gives the DATA only ~16 bit times after the token.  The PHY
            // buffers the next packet itself, so bound the gap by a timer that
            // stays under the device timeout (LS byte = 30 cycles).
            if (eop_wait) begin
                eop_to <= eop_to + 14'd1;
                if (rxcmd_stb && rxcmd[1:0] == 2'b00) eop_se0 <= 1'b1;
                if ((rxcmd_stb && eop_se0 && rxcmd[1:0] != 2'b00) ||
                    eop_to == (ls_mode ? 14'd200 : 14'd60))
                    eop_wait <= 1'b0;
            end

            // ---------------- packet TX FSM (runs when register idle) -----
            case (tstate)
            T_IDLE: begin
                if (tx_req) pending <= 1'b1;
                if ((tx_req || pending) && rstate == R_IDLE && bus_free && !eop_wait && flush_ok) begin
                    data_out <= 8'h40 | tx_pid;
                    dbg_tx_byte <= 8'h40 | tx_pid;
                    dbg_tx_stb  <= 1'b1;
                    out_req  <= 1'b1;
                    tidx     <= 4'd0;
                    pending  <= 1'b0;
                    tx_busy  <= 1'b1;
                    tstall   <= 12'd0;
                    tstate   <= T_CMD;
                end
            end

            T_CMD: begin
                tstall <= tstall + 12'd1;           // abort a stalled packet (PHY not responding)
                if (!bus_free) begin                // PHY interrupt (RXCMD): pause, keep the command
                    tstall <= 12'd0;
                end else if (tstall == 12'hFFF) begin
                    out_req <= 1'b0; data_out <= 8'h00; tx_abort_cnt <= tx_abort_cnt + 8'd1;
                    tx_busy <= 1'b0; tx_done <= 1'b1; tstate <= T_IDLE;
                end else if (!ulpi_nxt) begin
                    out_req <= 1'b1;                // hold TX CMD until the PHY takes it
                end else begin
                if (tx_len == 4'd0) begin
                    data_out   <= 8'h00;
                    ulpi_stp_r <= 1'b1;             // STP right after the final NXT
                    tstate     <= T_DONE;
                end else begin
                    data_out <= tbyte(4'd0);
                    dbg_tx_byte <= tbyte(4'd0);
                    dbg_tx_stb  <= 1'b1;
                    tidx     <= 4'd1;
                    tstall   <= 12'd0;
                    tstate   <= T_DATA;
                end
                end
            end

            T_DATA: begin
                tstall <= tstall + 12'd1;
                if (!bus_free) begin                // PHY interrupt (RXCMD): pause, resume this byte
                    tstall <= 12'd0;
                end else if (tstall == 12'hFFF) begin
                    out_req <= 1'b0; data_out <= 8'h00; tx_abort_cnt <= tx_abort_cnt + 8'd1;
                    tx_busy <= 1'b0; tx_done <= 1'b1; tstate <= T_IDLE;
                end else if (ulpi_nxt) begin
                if (tidx == tx_len) begin
                    data_out   <= 8'h00;
                    ulpi_stp_r <= 1'b1;             // STP right after the final NXT
                    tstate     <= T_DONE;
                end else begin
                    data_out <= tbyte(tidx);
                    dbg_tx_byte <= tbyte(tidx);
                    dbg_tx_stb  <= 1'b1;
                    tidx     <= tidx + 4'd1;
                    tstall   <= 12'd0;
                end
                end
            end

            T_STP: begin
                ulpi_stp_r <= 1'b1;
                tstate     <= T_DONE;
            end

            T_DONE: begin
                ulpi_stp_r <= 1'b0;
                out_req    <= 1'b0;
                eop_wait   <= 1'b1; eop_se0 <= 1'b0; eop_to <= 14'd0;
                tx_busy    <= 1'b0;
                tx_done    <= 1'b1;
                tstate     <= T_IDLE;
            end

            default: tstate <= T_IDLE;
            endcase

            if (r_req ^ r_req_d) hreq <= 1'b1;

            // an idle link (>= ~400 cycles) ends the transaction: the next one
            // must flush again
            if (tstate == T_IDLE && rstate == R_IDLE && !tx_req && !pending)
                idle_cnt <= (idle_cnt == 12'hFFF) ? idle_cnt : idle_cnt + 12'd1;
            else idle_cnt <= 12'd0;
            // flush disabled for the phase scan
        end
    end

    // ---------------- receive monitor (read-only) ----------------
    reg [7:0] rxbuf [0:31];
    reg [4:0] rxcnt;
    reg       rxact;
    // first DIR-high cycle is bus turnaround: data is still the link's last drive
    wire      turn   = ulpi_dir & ~dir_d;
    wire      rd_ret = (rstate == R_RD_WAIT) || (rstate == R_RD_DONE);
    assign    bus_free = !ulpi_dir && !dir_d;
    assign    dir_d_o  = dir_d;

    assign line_state = rxcmd[1:0];   // RXCMD: [1:0] LineState, [3:2] VbusState, [5:4] RxEvent
    assign rxcmd_raw  = rxcmd;

    integer k;
    reg pkt_end;
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            rxact    <= 1'b0;
            rxcnt    <= 5'd0;
            rx_valid <= 1'b0;
            rx_pid   <= 8'h00;
            rx_len   <= 5'd0;
            rx_error <= 1'b0;
            rxcmd    <= 8'h00;
            rxcmd_cnt <= 8'h00;
            rxcmd_stb <= 1'b0;
            dir_d    <= 1'b0;
            rx_data  <= 256'd0;
            rxstart_cnt  <= 8'd0;
            rxbyte_cnt   <= 8'd0;
            rxactcmd_cnt <= 8'd0;
            for (k = 0; k < 32; k = k + 1) rxbuf[k] <= 8'h00;
        end else begin
            rx_valid <= 1'b0;
            rxcmd_stb <= 1'b0;
            dir_d    <= ulpi_dir;
            if (rx_valid) rx_error <= 1'b0;
            // ULPI 1.1 receive protocol:
            //  * turnaround cycle (first DIR=1): bus content invalid.  NXT=1 on
            //    it means the PHY is starting a USB receive (RxActive).
            //  * DIR=1, NXT=1 : one received packet byte (PID first)
            //  * DIR=1, NXT=0 : RX CMD {.., RxEvent[5:4], Vbus[3:2], LineState[1:0]}
            //    RxEvent: 01 = RxActive, 11 = RxError, 00 = not active
            //  * end of packet: RX CMD with RxActive=0, or DIR dropping
            pkt_end = 1'b0;
            if (ulpi_dir) begin
                if (turn) begin
                    if (ulpi_nxt) begin
                        rxact    <= 1'b1;            // USB receive starts, no data yet
                        rxcnt    <= 5'd0;
                        rxstart_cnt <= rxstart_cnt + 8'd1;
                    end
                end else if (ulpi_nxt) begin
                    rxact <= 1'b1;
                    if (!rxact) begin
                        rxbuf[5'd0] <= ulpi_data;
                        rxcnt       <= 5'd1;
                    end else if (rxcnt < 5'd31) begin
                        rxbuf[rxcnt] <= ulpi_data;
                        rxcnt        <= rxcnt + 5'd1;
                    end
                    rxbyte_cnt <= rxbyte_cnt + 8'd1;
                end else if (!rd_ret) begin
                    rxcmd     <= ulpi_data;
                    rxcmd_stb <= 1'b1;
                    rxcmd_cnt <= rxcmd_cnt + 8'd1;
                    if (ulpi_data[5:4] == 2'b11) rx_error <= 1'b1;
                    if (ulpi_data[4]) rxactcmd_cnt <= rxactcmd_cnt + 8'd1;
                    if (rxact && !ulpi_data[4]) pkt_end = 1'b1;
                end
            end else if (rxact) begin
                pkt_end = 1'b1;
            end

            if (pkt_end && rxact && rxcnt == 5'd0) begin
                rxact <= 1'b0;                        // RxActive with no bytes: drop
            end else if (pkt_end && rxact) begin
                rxact    <= 1'b0;
                rx_valid <= 1'b1;
                rx_pid   <= rxbuf[5'd0];
                rx_len   <= rxcnt - 5'd1;
                rx_data  <= { rxbuf[31], rxbuf[30], rxbuf[29], rxbuf[28],
                              rxbuf[27], rxbuf[26], rxbuf[25], rxbuf[24],
                              rxbuf[23], rxbuf[22], rxbuf[21], rxbuf[20],
                              rxbuf[19], rxbuf[18], rxbuf[17], rxbuf[16],
                              rxbuf[15], rxbuf[14], rxbuf[13], rxbuf[12],
                              rxbuf[11], rxbuf[10], rxbuf[ 9], rxbuf[ 8],
                              rxbuf[ 7], rxbuf[ 6], rxbuf[ 5], rxbuf[ 4],
                              rxbuf[ 3], rxbuf[ 2], rxbuf[ 1], rxbuf[ 0] };
            end
        end
    end


    // change-only trace: {delta cycles since previous entry (sat), sample}
    (* ram_style = "block" *) reg [31:0] tmem [0:1023];
    reg [10:0] tptr;
    reg        tarm;
    reg [19:0] tlast;
    reg [11:0] tdelta;
    wire [19:0] tsample = {rstate, tstate, ulpi_dir, ulpi_nxt, ulpi_stp_r, out_req & ~ulpi_dir, ulpi_data};
    wire        twr = tarm && (tsample != tlast || tdelta == 12'hFFF);
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            tptr <= 11'd0; tarm <= 1'b0; trace_done <= 1'b0; tlast <= 20'd0; tdelta <= 12'd0;
        end else begin
            if (!tarm && !trace_done && tstate == T_IDLE && (tx_req || pending) &&
                rstate == R_IDLE && bus_free && tx_pid == TRIG_PID) begin
                tarm <= 1'b1; tlast <= ~tsample; tdelta <= 12'd0;
            end
            if (tarm) begin
                if (twr) begin
                    tlast <= tsample; tdelta <= 12'd1;
                    tptr <= tptr + 11'd1;
                    if (tptr == 11'd1023) begin tarm <= 1'b0; trace_done <= 1'b1; end
                end else tdelta <= tdelta + 12'd1;
            end
        end
    end
    always @(posedge clk) begin
        if (twr) tmem[tptr[9:0]] <= {tdelta, tsample};
        trace_q <= tmem[trace_addr];
    end
endmodule
