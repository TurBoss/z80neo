// ============================================================================
// ulpi_host - USB low/full-speed host over the ULPI link (keyboard targeted)
//
//   * PHY register init, device detect + speed from Debug reg LineState
//     (01 = D+ pull-up = full speed, 10 = D- pull-up = low speed)
//   * USB bus reset (HS xcvr + termination -> SE0), then FS or LS host transceiver
//   * every 1 ms at a transaction boundary: SOF (FS) or keep-alive EOP (LS)
//   * control transfers: SETUP/DATA0/IN/OUT with CRC5/CRC16, ACK handshakes
//   * enumeration step table (GET_DESCRIPTOR / SET_ADDRESS / ...)
//   * HID interrupt-IN polling on ep1, boot-report -> key code
//
// Packet bytes carry their own CRC (the ULPI PHY does not generate CRCs).
// All logic runs on ~ulpi_clk (this board's ULPI clock is inverted).
// ============================================================================
module ulpi_host #(
    parameter integer CLK_HZ      = 60000000,
    parameter integer RESET_CYCLES  = 900000,
    parameter integer SETTLE_CYCLES = 3000000,
    parameter integer DETECT_CYCLES = 6000000,
    parameter integer ADDR_CYCLES   = 300000,
    parameter [7:0]   OTG_INIT      = 8'h26,   // Dp/Dm pulldown + DRVVBUS
    parameter [7:0]   FC_DETECT     = 8'h45,   // FS, TermSelect
    // ULPI host bus reset: HS transceiver + HS termination (XcvrSelect=00,
    // TermSelect=0, OpMode=00).  The 45R terminations pull D+/D- to SE0.
    // (OpMode=10 only disables bit-stuff/NRZI -- it drives nothing.)
    parameter [7:0]   RESET_FS      = 8'h40,
    parameter [7:0]   RESET_LS      = 8'h40,
    parameter [7:0]   FC_FS         = 8'h41,   // FS, no HS term
    parameter [7:0]   FC_LS         = 8'h42,   // LS, no HS term
    parameter [1:0]   FORCE_SPEED   = 2'd2,    // 2=auto, 0=force FS, 1=force LS
    parameter         LOOSE_ACK     = 1'b0,    // 1 = accept any RX as ACK (debug)
    parameter         DBGSEL        = 0
)(
    input         clk,
    input         rst_n,

    inout  [7:0]  ulpi_data,
    input         ulpi_dir,
    input         ulpi_nxt,
    output        ulpi_stp,

    output [63:0] dbg,
    output [7:0]  dbg_tx_byte,
    output        dbg_tx_stb,
    input      [9:0] trace_addr,
    output    [31:0] trace_q,
    output           trace_done,
    output reg [7:0] hid_key,
    output reg [7:0] hid_mod,
    output reg       hid_key_valid,
    output [127:0]   dbg2
);
    `include "usb_crc.vh"

    // PID values
    localparam [3:0] PID_OUT = 4'h1, PID_IN = 4'h9, PID_SOF = 4'h5,
                     PID_SETUP = 4'hD, PID_ACK = 4'h2, PID_NAK = 4'hA,
                     PID_DATA0 = 4'h3, PID_DATA1 = 4'hB;

    // ---------------------------------------------------------------- link
    reg         r_wr;
    reg  [5:0]  r_addr;
    reg  [7:0]  r_wdata;
    wire [7:0]  r_rdata;
    wire        r_done;

    reg         tx_req;
    reg  [3:0]  tx_pid, tx_len;
    reg  [95:0] tx_data;
    wire        tx_busy, tx_done;

    wire [7:0]  rx_pid;
    wire [4:0]  rx_len;
    wire [255:0] rx_data;
    wire        rx_valid, rx_error;
    wire [1:0]  line_state;
    wire [3:0]  link_state;
    wire [7:0]  rxcmd_raw, rxcmd_cnt, tx_abort_cnt, tx_dir_cnt;
    wire [7:0]  rxstart_cnt, rxbyte_cnt, rxactcmd_cnt;
    reg  [3:0]  max_step;
    reg         ls;              // low-speed device attached
    reg  [7:0]  nak_cnt, err_cnt, to_cnt;
    reg  [7:0]  scr_err, scr_pat;
    reg  [6:0]  scr_n;
    reg [7:0]   rd_cnt;
    reg [7:0]   rs_cnt;
    reg         ready;
    reg [7:0]   rx_cnt;
    reg [7:0]   tx_cnt;

    reg r_go_t;
    ulpi_link u_link (
        .clk(clk), .rst_n(rst_n),
        .ulpi_data(ulpi_data), .ulpi_dir(ulpi_dir), .ulpi_nxt(ulpi_nxt),
        .ulpi_stp(ulpi_stp),
        .r_req(r_go_t), .r_wr(r_wr), .r_addr(r_addr), .r_wdata(r_wdata),
        .r_rdata(r_rdata), .r_done(r_done), .r_busy(),
        .tx_req(tx_req), .tx_pid(tx_pid), .tx_len(tx_len), .tx_data(tx_data),
        .tx_busy(tx_busy), .tx_done(tx_done),
        .rx_pid(rx_pid), .rx_len(rx_len), .rx_data(rx_data),
        .rx_valid(rx_valid), .rx_error(rx_error),
        .line_state(line_state), .rxcmd_raw(rxcmd_raw), .rxcmd_cnt(rxcmd_cnt),
        .tx_abort_cnt(tx_abort_cnt), .tx_dir_cnt(tx_dir_cnt),
        .rxstart_cnt(rxstart_cnt), .rxbyte_cnt(rxbyte_cnt), .rxactcmd_cnt(rxactcmd_cnt),
        .ls_mode(ls), .trace_addr(trace_addr), .trace_q(trace_q), .trace_done(trace_done),
        .dbg_state(link_state),
        .dbg_tx_byte(dbg_tx_byte), .dbg_tx_stb(dbg_tx_stb)
    );

    // ------------------------------------------------------- tx helpers
    // Token/SOF body = 2 bytes, LSB first: field[7:0], {CRC5, field[10:8]}
    // (SETUP addr0/ep0 -> 00 10, IN addr1/ep0 -> 01 E8).
    function [95:0] tok_field_word;
        input [10:0] f;
        reg [4:0]  c;
        begin
            c = usb_crc5_field(f);
            tok_field_word = {80'd0, c, f[10:8], f[7:0]};
        end
    endfunction

    // Build a token packet from addr/ep.
    function [95:0] tok_word;
        input [6:0] a;
        input [3:0] e;
        begin
            tok_word = tok_field_word({e, a});
        end
    endfunction

    // Build a DATA packet: n payload bytes (from w, MSB-first list) + CRC16.
    function [95:0] dat_word;
        input [63:0] w;
        input [3:0]  n;
        reg [15:0] c;
        integer i;
        begin
            c = usb_crc16_word(w, n);
            dat_word = 96'd0;
            // bytes 0..n-1 = payload (LSB byte first)
            for (i = 0; i < 8; i = i + 1)
                if (i < n) dat_word[i*8 +: 8] = w[(7-i)*8 +: 8];
            dat_word[n*8 +: 8]  = c[7:0];
            dat_word[n*8+8 +: 8] = c[15:8];
        end
    endfunction

    // ------------------------------------------------------- main FSM
    localparam [5:0]
        S_OTG      = 6'd0,  S_OTG_W    = 6'd1,  S_FC1      = 6'd2,  S_FC1_W    = 6'd3,
        S_RSTW     = 6'd4,  S_FC2      = 6'd5,  S_FC2_W    = 6'd6,  S_SETTLE   = 6'd7,
        S_NEXT     = 6'd8,
        S_SET_T    = 6'd9,  S_SET_TW   = 6'd10, S_SET_DW   = 6'd12, S_SET_RXW  = 6'd14,
        S_RXD_T    = 6'd15, S_RXD_TW   = 6'd16, S_RXD_RXW  = 6'd18,
        S_RXD_ACK  = 6'd19, S_RXD_ACKW = 6'd20,
        S_STS_T    = 6'd21, S_STS_TW   = 6'd22, S_STS_DW   = 6'd24, S_STS_RXW  = 6'd26,
        S_STI_T    = 6'd27, S_STI_TW   = 6'd28, S_STI_RXW  = 6'd30,
        S_STI_ACK  = 6'd31, S_STI_ACKW = 6'd32,
        S_STEP     = 6'd33,
        S_POL_T    = 6'd34, S_POL_TW   = 6'd35, S_POL_RXW  = 6'd37,
        S_POL_ACK  = 6'd38, S_POL_ACKW = 6'd39,
        S_FCD      = 6'd41, S_FCD_W    = 6'd42, S_DET      = 6'd43, S_DET_W    = 6'd44,
        S_DETWAIT  = 6'd45, S_SOF      = 6'd46, S_SOFW     = 6'd47, S_ADDRW    = 6'd48,
        S_RSTC     = 6'd49, S_RSTC_W   = 6'd50,
        S_TW       = 6'd51, S_TW_W     = 6'd52, S_TR       = 6'd53, S_TR_W     = 6'd54;

    reg [5:0]  st, ret_st;
    reg [31:0] dwell;
    reg [15:0] rx_to;
    reg [3:0]  step;
    reg [7:0]  setup [0:7];
    reg        want_in;
    reg [3:0]  in_len;
    reg [7:0]  dev_addr;
    reg [7:0]  rxbuf0;
    reg [10:0] frame;
    reg [3:0]  retry;
    reg [1:0]  det_ls;           // LineState seen at detect
    reg [1:0]  rst_ls;           // LineState read during bus reset (expect 00)
    reg [7:0]  prev_key, last_key;

    // 1 ms frame tick -> toggle; pending while sof_t != sof_ack
    reg        sof_t, sof_ack;
    reg [15:0] sof_cnt;
    wire       sof_pend = (sof_t ^ sof_ack) & ready;
    always @(posedge clk or negedge rst_n)
        if (!rst_n) begin sof_cnt <= 16'd0; sof_t <= 1'b0; end
        else if (sof_cnt == 16'd59999) begin sof_cnt <= 16'd0; sof_t <= ~sof_t; end
        else sof_cnt <= sof_cnt + 16'd1;

    // register op helper (toggle handshake into the link)
    reg rw_go, rw_w;
    reg [5:0] rw_a;
    reg [7:0] rw_d;
    always @(posedge clk or negedge rst_n)
        if (!rst_n) begin r_go_t <= 1'b0; r_wr <= 1'b0; r_addr <= 6'd0; r_wdata <= 8'd0; end
        else if (rw_go) begin r_addr <= rw_a; r_wdata <= rw_d; r_wr <= rw_w; r_go_t <= ~r_go_t; end

    // step table: fill setup[]/want_in/in_len for step
    task set_step;
        input [3:0] s;
        begin
            setup[0] <= 8'h00; setup[1] <= 8'h00; setup[2] <= 8'h00; setup[3] <= 8'h00;
            setup[4] <= 8'h00; setup[5] <= 8'h00; setup[6] <= 8'h00; setup[7] <= 8'h00;
            want_in  <= 1'b0;
            in_len   <= 4'd0;
            case (s)
            4'd0: begin // GET_DESCRIPTOR(device, 8)
                setup[0]<=8'h80; setup[1]<=8'h06; setup[2]<=8'h00; setup[3]<=8'h01;
                setup[4]<=8'h00; setup[5]<=8'h00; setup[6]<=8'h08; setup[7]<=8'h00;
                want_in <= 1'b1; in_len <= 4'd8;
            end
            4'd1: begin // SET_ADDRESS(1)
                setup[0]<=8'h00; setup[1]<=8'h05; setup[2]<=8'h01; setup[3]<=8'h00;
            end
            4'd2: begin // GET_DESCRIPTOR(device, 8) - one ep0 packet
                setup[0]<=8'h80; setup[1]<=8'h06; setup[2]<=8'h00; setup[3]<=8'h01;
                setup[4]<=8'h00; setup[5]<=8'h00; setup[6]<=8'h08; setup[7]<=8'h00;
                want_in <= 1'b1; in_len <= 4'd8;
            end
            4'd3: begin // GET_DESCRIPTOR(config, 8) - one ep0 packet
                setup[0]<=8'h80; setup[1]<=8'h06; setup[2]<=8'h00; setup[3]<=8'h02;
                setup[4]<=8'h00; setup[5]<=8'h00; setup[6]<=8'h08; setup[7]<=8'h00;
                want_in <= 1'b1; in_len <= 4'd8;
            end
            4'd4: begin // SET_CONFIGURATION(1)
                setup[0]<=8'h00; setup[1]<=8'h09; setup[2]<=8'h01; setup[3]<=8'h00;
            end
            4'd5: begin // SET_IDLE (HID class, interface 0)
                setup[0]<=8'h21; setup[1]<=8'h0A; setup[2]<=8'h00; setup[3]<=8'h00;
                setup[4]<=8'h00; setup[5]<=8'h00; setup[6]<=8'h00; setup[7]<=8'h00;
            end
            default: ;
            endcase
        end
    endtask

    wire ack_ok  = rx_valid && (LOOSE_ACK | (rx_pid[3:0] == PID_ACK));
    wire nak_rx  = rx_valid && (rx_pid[3:0] == PID_NAK);
    wire data_ok = rx_valid && (LOOSE_ACK | (rx_pid[3:0] == PID_DATA0) || (rx_pid[3:0] == PID_DATA1));

    // rx_data[7:0] is the PID; payload byte n is rx_data[8*(n+1) +: 8]
    wire [7:0] rep_key0 = rx_data[31:24];   // boot report byte 2
    wire [7:0] rep_key1 = rx_data[39:32];   // boot report byte 3

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            st <= S_TW; ret_st <= S_OTG; dwell <= 32'd0; rx_to <= 16'd0;
            scr_err <= 8'd0; scr_pat <= 8'h5A; scr_n <= 7'd0;
            tx_req <= 1'b0; tx_pid <= 4'd0; tx_len <= 4'd0; tx_data <= 96'd0;
            rw_go <= 1'b0; rw_w <= 1'b1; rw_a <= 6'd0; rw_d <= 8'd0;
            step <= 4'd0; dev_addr <= 8'd0; want_in <= 1'b0; in_len <= 4'd0;
            hid_key <= 8'd0; hid_mod <= 8'd0; hid_key_valid <= 1'b0; prev_key <= 8'd0; last_key <= 8'd0;
            frame <= 11'd0; retry <= 4'd0; rd_cnt <= 8'd0; rs_cnt <= 8'd0; ready <= 1'b0;
            rx_cnt <= 8'd0; tx_cnt <= 8'd0; rxbuf0 <= 8'd0; ls <= 1'b0; det_ls <= 2'd0; rst_ls <= 2'd3;
            sof_ack <= 1'b0;
            max_step <= 4'd0; nak_cnt <= 8'd0; err_cnt <= 8'd0; to_cnt <= 8'd0;
        end else begin
            hid_key_valid <= 1'b0;
            tx_req <= 1'b0;
            rw_go  <= 1'b0;
            if (r_done)   rd_cnt <= rd_cnt + 8'd1;
            if (rx_valid) rx_cnt <= rx_cnt + 8'd1;
            if (tx_done)  tx_cnt <= tx_cnt + 8'd1;
            if (step > max_step) max_step <= step;
            if (nak_rx) nak_cnt <= nak_cnt + 8'd1;
            if (rx_valid && !ack_ok && !nak_rx && !data_ok) err_cnt <= err_cnt + 8'd1;
            if ((st == S_SET_RXW || st == S_RXD_RXW || st == S_STS_RXW || st == S_STI_RXW) &&
                rx_to == 16'd0 && !rx_valid) to_cnt <= to_cnt + 8'd1;

            case (st)
            // ---------------- ULPI timing self test: scratch reg 0x16 ----------
            S_TW:   begin rw_w <= 1'b1; rw_a <= 6'h16; rw_d <= scr_pat; rw_go <= 1'b1; st <= S_TW_W; end
            S_TW_W: if (r_done) st <= S_TR;
            S_TR:   begin rw_w <= 1'b0; rw_a <= 6'h16; rw_go <= 1'b1; st <= S_TR_W; end
            S_TR_W: if (r_done) begin
                        if (r_rdata != scr_pat && scr_err != 8'hFF) scr_err <= scr_err + 8'd1;
                        scr_pat <= {scr_pat[6:0], scr_pat[7] ^ scr_pat[5] ^ scr_pat[4] ^ scr_pat[3]} ^ 8'h01;
                        scr_n <= scr_n + 7'd1;
                        st <= (scr_n == 7'd63) ? S_OTG : S_TW;
                    end

            // ---------------- PHY init + detect ----------------
            S_OTG:   begin ready <= 1'b0; rw_w <= 1'b1; rw_a <= 6'h0A; rw_d <= OTG_INIT; rw_go <= 1'b1; st <= S_OTG_W; end
            S_OTG_W: if (r_done) st <= S_FCD;                          // DP/DM 15k pulldowns
            S_FCD:   begin rw_w <= 1'b1; rw_a <= 6'h04; rw_d <= FC_DETECT; rw_go <= 1'b1; st <= S_FCD_W; end
            S_FCD_W: if (r_done) begin dwell <= 32'd60000; st <= S_DETWAIT; end
            S_DETWAIT: if (dwell == 0) st <= S_DET; else dwell <= dwell - 32'd1;
            S_DET:   begin rw_w <= 1'b0; rw_a <= 6'h15; rw_go <= 1'b1; st <= S_DET_W; end  // Debug: LineState
            S_DET_W: if (r_done) begin
                         det_ls <= r_rdata[1:0];
                         if (r_rdata[1:0] == 2'b01 || r_rdata[1:0] == 2'b10) begin
                             ls <= (FORCE_SPEED == 2'd2) ? (r_rdata[1:0] == 2'b10) : FORCE_SPEED[0];
                             st <= S_FC1;
                         end else begin                        // nothing attached: poll again
                             dwell <= DETECT_CYCLES; st <= S_DETWAIT;
                         end
                     end

            // ---------------- bus reset, then host transceiver ----------------
            // USB reset = SE0: HS transceiver, HS termination on, normal OpMode
            // (XcvrSelect=00, TermSelect=0, OpMode=00). The 45R terminations
            // pull D+/D- low. Measured on the USB3317: OpMode=10 (0x50) leaves
            // the terminations off and the line stays J. LineState then reads
            // in HS squelch terms (01 = !squelch).
            // USB bus reset: Function Control OpMode=10b drives SE0 on D+/D- for
            // the reset window (XcvrSelect=HS).  0x40 was normal/no-drive and a
            // device never sees a reset, so it ignores all traffic.
            S_FC1:   begin rw_w <= 1'b1; rw_a <= 6'h04; rw_d <= (ls ? RESET_LS : RESET_FS); rw_go <= 1'b1; st <= S_FC1_W; end
            S_FC1_W: if (r_done) begin dwell <= 32'd60000; st <= S_RSTC; end
            S_RSTC:  if (dwell == 0) begin rw_w <= 1'b0; rw_a <= 6'h15; rw_go <= 1'b1; st <= S_RSTC_W; end
                     else dwell <= dwell - 32'd1;
            S_RSTC_W: if (r_done) begin rst_ls <= r_rdata[1:0]; dwell <= RESET_CYCLES; st <= S_RSTW; end
            S_RSTW:  if (dwell == 0) st <= S_FC2; else dwell <= dwell - 32'd1;
            S_FC2:   begin rw_w <= 1'b1; rw_a <= 6'h04; rw_d <= ls ? FC_LS : FC_FS; rw_go <= 1'b1; st <= S_FC2_W; end
            S_FC2_W: if (r_done) begin dwell <= SETTLE_CYCLES; dev_addr <= 8'd0; ready <= 1'b1; st <= S_SETTLE; end
            S_SETTLE: if (sof_pend) begin ret_st <= S_SETTLE; st <= S_SOF; end
                      else if (dwell == 0) begin step <= 4'd0; st <= S_NEXT; end
                      else dwell <= dwell - 32'd1;

            // ---------------- frame marker: SOF (FS) / keep-alive (LS) ----------------
            S_SOF: begin
                // SOF in both speeds: in LS host mode the PHY turns it into the
                // keep-alive EOP; either way it keeps the device out of suspend
                sof_ack <= sof_t;
                tx_pid  <= PID_SOF; tx_len <= 4'd2;
                tx_data <= tok_field_word(frame);
                frame   <= frame + 11'd1;
                tx_req  <= 1'b1; st <= S_SOFW;
            end
            S_SOFW: if (tx_done) st <= ret_st;

            // ---------------- enumeration step dispatch ----------------
            S_NEXT: begin
                if (step == 4'd6) st <= S_POL_T;
                else begin set_step(step); retry <= 4'hF; st <= S_SET_T; end
            end

            // SETUP token + DATA0(8)
            S_SET_T: if (sof_pend) begin ret_st <= S_SET_T; st <= S_SOF; end
                     else begin tx_pid <= PID_SETUP; tx_len <= 4'd2;
                          tx_data <= tok_word(dev_addr[6:0], 4'd0); tx_req <= 1'b1; st <= S_SET_TW; end
            S_SET_TW: if (tx_done) begin
                          tx_pid <= PID_DATA0; tx_len <= 4'd10;
                          tx_data <= dat_word({setup[0],setup[1],setup[2],setup[3],
                                               setup[4],setup[5],setup[6],setup[7]}, 4'd8);
                          tx_req <= 1'b1; st <= S_SET_DW;
                      end
            S_SET_DW: if (tx_done) begin rx_to <= 16'hFFFF; st <= S_SET_RXW; end
            S_SET_RXW: if (ack_ok) begin
                           retry <= 4'hF;
                           st <= want_in ? S_RXD_T : S_STI_T;
                       end else if (rx_valid || rx_to == 0) begin
                           if (retry == 4'd0) st <= S_OTG;                 // give up: re-detect + reset
                           else begin retry <= retry - 4'd1; st <= S_SET_T; end
                       end else rx_to <= rx_to - 16'd1;

            // IN data stage (first packet)
            S_RXD_T: if (sof_pend) begin ret_st <= S_RXD_T; st <= S_SOF; end
                     else begin tx_pid <= PID_IN; tx_len <= 4'd2;
                          tx_data <= tok_word(dev_addr[6:0], 4'd0); tx_req <= 1'b1; st <= S_RXD_TW; end
            S_RXD_TW: if (tx_done) begin rx_to <= 16'hFFFF; st <= S_RXD_RXW; end
            S_RXD_RXW: if (data_ok) begin
                           if (step == 4'd0) rxbuf0 <= rx_data[15:8];      // bLength (0x12)
                           st <= S_RXD_ACK;
                       end else if (rx_valid || rx_to == 0) begin      // NAK / error / timeout
                           if (retry == 4'd0) st <= S_OTG;
                           else begin retry <= retry - 4'd1; st <= S_RXD_T; end
                       end else rx_to <= rx_to - 16'd1;
            S_RXD_ACK: begin tx_pid <= PID_ACK; tx_len <= 4'd0; tx_data <= 96'd0; tx_req <= 1'b1; st <= S_RXD_ACKW; end
            S_RXD_ACKW: if (tx_done) begin retry <= 4'hF; st <= S_STS_T; end

            // status for control-IN: OUT + zero-length DATA1, expect ACK
            S_STS_T: if (sof_pend) begin ret_st <= S_STS_T; st <= S_SOF; end
                     else begin tx_pid <= PID_OUT; tx_len <= 4'd2;
                          tx_data <= tok_word(dev_addr[6:0], 4'd0); tx_req <= 1'b1; st <= S_STS_TW; end
            S_STS_TW: if (tx_done) begin
                          tx_pid <= PID_DATA1; tx_len <= 4'd2;
                          tx_data <= dat_word(64'd0, 4'd0);
                          tx_req <= 1'b1; st <= S_STS_DW;
                      end
            S_STS_DW: if (tx_done) begin rx_to <= 16'hFFFF; st <= S_STS_RXW; end
            S_STS_RXW: if (ack_ok) st <= S_STEP;
                       else if (rx_valid || rx_to == 0) begin
                           if (retry == 4'd0) st <= S_OTG;
                           else begin retry <= retry - 4'd1; st <= S_STS_T; end
                       end else rx_to <= rx_to - 16'd1;

            // status for no-data control: IN, expect zero-length DATA1
            S_STI_T: if (sof_pend) begin ret_st <= S_STI_T; st <= S_SOF; end
                     else begin tx_pid <= PID_IN; tx_len <= 4'd2;
                          tx_data <= tok_word(dev_addr[6:0], 4'd0); tx_req <= 1'b1; st <= S_STI_TW; end
            S_STI_TW: if (tx_done) begin rx_to <= 16'hFFFF; st <= S_STI_RXW; end
            S_STI_RXW: if (data_ok) st <= S_STI_ACK;
                       else if (rx_valid || rx_to == 0) begin
                           if (retry == 4'd0) st <= S_OTG;
                           else begin retry <= retry - 4'd1; st <= S_STI_T; end
                       end else rx_to <= rx_to - 16'd1;
            S_STI_ACK: begin tx_pid <= PID_ACK; tx_len <= 4'd0; tx_data <= 96'd0; tx_req <= 1'b1; st <= S_STI_ACKW; end
            S_STI_ACKW: if (tx_done) st <= S_STEP;

            S_STEP: begin
                step <= step + 4'd1;
                if (step == 4'd1) begin                         // SET_ADDRESS: 2 ms+ recovery
                    dev_addr <= 8'd1; dwell <= ADDR_CYCLES; st <= S_ADDRW;
                end else st <= S_NEXT;
            end
            S_ADDRW: if (sof_pend) begin ret_st <= S_ADDRW; st <= S_SOF; end
                     else if (dwell == 0) st <= S_NEXT;
                     else dwell <= dwell - 32'd1;

            // ---------------- HID interrupt-IN polling (ep1) ----------------
            S_POL_T: if (sof_pend) begin ret_st <= S_POL_T; st <= S_SOF; end
                     else begin tx_pid <= PID_IN; tx_len <= 4'd2;
                          tx_data <= tok_word(dev_addr[6:0], 4'd1); tx_req <= 1'b1; st <= S_POL_TW; end
            S_POL_TW: if (tx_done) begin rx_to <= 16'h3FFF; st <= S_POL_RXW; end
            S_POL_RXW: if (data_ok) begin
                           // new key press = first key code not present in the previous report
                           if (rep_key0 != 8'h00 && rep_key0 != prev_key) begin
                               hid_key <= rep_key0; hid_mod <= rx_data[15:8];
                               hid_key_valid <= 1'b1; last_key <= rep_key0;
                           end
                           prev_key <= rep_key0;
                           st <= S_POL_ACK;
                       end else if (rx_valid || rx_to == 0) st <= S_POL_T;
                       else rx_to <= rx_to - 16'd1;
            S_POL_ACK: begin tx_pid <= PID_ACK; tx_len <= 4'd0; tx_data <= 96'd0; tx_req <= 1'b1; st <= S_POL_ACKW; end
            S_POL_ACKW: if (tx_done) st <= S_POL_T;

            default: st <= S_OTG;
            endcase
        end
    end

    // ------------------------------------------------------- debug pack
    // [63:56] last key  [55:48] rx_cnt  [47:40] tx_cnt  [39:32] {ls,3'b0,step}
    // [31:24] {2'b0,st} [23:16] rx_pid  [15:8] bLength  [7:0] {det_ls,2'b0,retry}
    // status line for the UART dumper (12 bytes, byte 0 printed first)
    // byte order printed: st step/max ls rx tx txabort rxstart rxbytes rxactcmd
    //                     rxcmd rx_pid nak err timeout retry last_key
    assign dbg2 = { last_key, scr_err, to_cnt, err_cnt, nak_cnt, rx_pid, rxcmd_raw,
                    rxactcmd_cnt, rxbyte_cnt, rxstart_cnt, tx_abort_cnt, tx_cnt, rx_cnt,
                    {det_ls, rst_ls, ls, 3'b0}, {max_step, step}, {2'b0, st} };
    assign dbg = DBGSEL ? { rxcmd_cnt, rxcmd_raw, tx_cnt, tx_abort_cnt, tx_dir_cnt,
                            rx_cnt, {2'b0, st}, {ls, 3'b0, step} }
                        : { last_key, rx_cnt, tx_cnt, {ls, 3'b0, step}, {2'b0, st},
                            rx_pid, rxbuf0, {det_ls, rst_ls, retry} };
endmodule
