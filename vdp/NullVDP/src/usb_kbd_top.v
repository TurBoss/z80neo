// ============================================================================
// usb_kbd_top - standalone USB keyboard bring-up (USB3317 ULPI host, no HDMI)
//
// The HDMI TMDS pins share IO banks with the ULPI pins, so this bitstream has
// no video.  The link runs on a PLL copy of the PHY's 60 MHz ULPI clock with a
// run-time selectable phase (16 x 22.5 deg).  With SCAN=1 the phase advances
// every ~4 s (PHY + host are reset on each change) so the working ULPI timing
// window can be measured; with SCAN=0 the phase is fixed at PHASE.
//
// UART (115200 8N1) prints a status line 4x per second:
//   bytes 0..15 = ulpi_host dbg2, 16 = {phase, 3'b0, pll_lock}
// LED3 blink code: 1 no device, 2 detected, 3 RX seen, 4 polling, 5 key.
// ============================================================================
module usb_kbd_top #(
    parameter       SCAN  = 1,
    parameter [3:0] PHASE = 4'd0
) (
    input        clk_27m,
    input        rst_n,
    input        ulpi_clk,
    input        ulpi_dir,
    input        ulpi_nxt,
    inout  [7:0] ulpi_data,
    output       ulpi_stp,
    output       ulpi_rst,
    output       uart_tx,
    output [3:0] led
);
    // ------------------------------------------------ 27 MHz housekeeping
    reg [7:0] pcnt;
    always @(posedge clk_27m or negedge rst_n)
        if (!rst_n) pcnt <= 8'd0;
        else if (pcnt != 8'hFF) pcnt <= pcnt + 8'd1;
    wire clk27_rst_n = rst_n & (pcnt == 8'hFF);

    // phase scan: every 2^27 cycles (~5 s) step the phase; each step holds the
    // PHY in reset for ~1 ms and the host for ~10 ms
    reg [26:0] scan_t;
    reg [3:0]  phase;
    reg [18:0] rst_t;                         // counts up after each change
    always @(posedge clk_27m or negedge clk27_rst_n)
        if (!clk27_rst_n) begin scan_t <= 27'd0; phase <= PHASE; rst_t <= 19'd0; end
        else begin
            scan_t <= scan_t + 27'd1;
            if (SCAN && scan_t == {27{1'b1}}) begin phase <= phase + 4'd1; rst_t <= 19'd0; end
            else if (rst_t != {19{1'b1}}) rst_t <= rst_t + 19'd1;
        end
    wire phy_run  = (rst_t > 19'd27000);      // PHY reset released after ~1 ms
    wire host_run = (rst_t == {19{1'b1}});    // host released after ~19 ms
    assign ulpi_rst = phy_run;                // board: high = PHY running

    // ------------------------------------------------ ULPI clock PLL
    wire ulclk, pll_lock;
    rPLL #(
        .FCLKIN("60"), .DYN_IDIV_SEL("false"), .IDIV_SEL(0),
        .DYN_FBDIV_SEL("false"), .FBDIV_SEL(0),
        .DYN_ODIV_SEL("false"), .ODIV_SEL(16),          // VCO 960 MHz
        .PSDA_SEL("0000"), .DYN_DA_EN("true"), .DUTYDA_SEL("1000"),
        .CLKOUT_FT_DIR(1'b1), .CLKOUTP_FT_DIR(1'b1),
        .CLKOUT_DLY_STEP(0), .CLKOUTP_DLY_STEP(0),
        .CLKFB_SEL("internal"), .CLKOUT_BYPASS("false"),
        .CLKOUTP_BYPASS("false"), .CLKOUTD_BYPASS("false"),
        .DYN_SDIV_SEL(2), .CLKOUTD_SRC("CLKOUT"), .CLKOUTD3_SRC("CLKOUT"),
        .DEVICE("GW2A-18")
    ) u_upll (
        .CLKOUT(), .LOCK(pll_lock), .CLKOUTP(ulclk), .CLKOUTD(), .CLKOUTD3(),
        .RESET(1'b0), .RESET_P(1'b0), .CLKIN(ulpi_clk), .CLKFB(1'b0),
        .FBDSEL(6'd0), .IDSEL(6'd0), .ODSEL(6'd0),
        .PSDA(phase), .DUTYDA(4'd8), .FDLY(4'd0)
    );

    // host reset, synchronised into the ULPI clock domain
    reg [2:0] ursync;
    wire      ur_async = host_run & pll_lock & clk27_rst_n;
    always @(posedge ulclk or negedge ur_async)
        if (!ur_async) ursync <= 3'd0;
        else           ursync <= {ursync[1:0], 1'b1};
    wire ul_rst_n = ursync[2];

    // ------------------------------------------------ USB host
    wire [63:0]  dbg;
    wire [127:0] dbg2;
    reg  [9:0]   tr_addr;
    wire [31:0]  tr_q;
    wire         tr_done;
    wire [7:0]   hid_key, hid_mod;
    wire         hid_key_valid;
    ulpi_host #(.FORCE_SPEED(2'd2),
                .OTG_INIT(8'h26),      // Dp/Dm pulldowns + DrvVbus
                .FC_DETECT(8'h45), .FC_FS(8'h45), .FC_LS(8'h46)) u_usb (
        .clk(ulclk), .rst_n(ul_rst_n),
        .ulpi_data(ulpi_data), .ulpi_dir(ulpi_dir), .ulpi_nxt(ulpi_nxt),
        .ulpi_stp(ulpi_stp),
        .dbg(dbg), .dbg_tx_byte(), .dbg_tx_stb(),
        .trace_addr(tr_addr), .trace_q(tr_q), .trace_done(tr_done),
        .hid_key(hid_key), .hid_mod(hid_mod), .hid_key_valid(hid_key_valid),
        .dbg2(dbg2)
    );
    wire        usb_dev_seen = (dbg[7:6] != 2'b00);
    wire        usb_polling  = (dbg[35:32] == 4'd6);
    wire        any_rx       = (dbg[55:48] != 8'd0);

    wire [7:0] cdc_key, cdc_mod;
    wire       cdc_key_valid;
    usb_key_cdc u_kcdc (
        .clk_in(ulclk), .rst_in_n(ul_rst_n),
        .key_in(hid_key), .mod_in(hid_mod), .valid_in(hid_key_valid),
        .clk_out(clk_27m), .rst_out_n(clk27_rst_n),
        .key_out(cdc_key), .mod_out(cdc_mod), .valid_out(cdc_key_valid)
    );
    reg key_seen;
    always @(posedge clk_27m or negedge clk27_rst_n)
        if (!clk27_rst_n) key_seen <= 1'b0;
        else if (cdc_key_valid) key_seen <= 1'b1;

    // ------------------------------------------------ status UART
    reg [127:0] dbg2_s1, dbg2_s2;
    reg [2:0]   lk_s;
    always @(posedge clk_27m) begin
        dbg2_s1 <= dbg2; dbg2_s2 <= dbg2_s1; lk_s <= {lk_s[1:0], pll_lock};
    end
    wire [7:0] sdata; wire svalid; wire tready;
    usb_stat_uart #(.N(17)) u_stat (
        .clk(clk_27m), .rst_n(clk27_rst_n),
        .v({ {phase, 3'b000, lk_s[2]}, dbg2_s2 }),
        .tx_data(sdata), .tx_valid(svalid), .tx_ready(tready & ~tr_mode)
    );
    uart_tx #(.CLK_FRE(27), .BAUD_RATE(115200)) u_tx (
        .clk(clk_27m), .rst_n(clk27_rst_n), .tx_pin(uart_tx),
        .tx_data(tdata), .tx_data_valid(tvalid), .tx_data_ready(tready), .tx_busy()
    );

    // one-shot dump of the ULPI cycle trace (first SETUP): 1024 x "8 hex\n"
    reg [2:0]  trd_s;
    always @(posedge clk_27m) trd_s <= {trd_s[1:0], tr_done};
    reg        tr_mode, tr_fin, tr_v;
    reg [3:0]  tr_ci, tr_wait;
    reg [31:0] tr_w;
    reg [7:0]  tr_d;
    function [7:0] hx; input [3:0] n; begin hx = (n < 10) ? (8'h30 + n) : (8'h37 + n); end endfunction
    always @(posedge clk_27m or negedge clk27_rst_n) begin
        if (!clk27_rst_n) begin
            tr_mode <= 1'b0; tr_fin <= 1'b0; tr_v <= 1'b0; tr_ci <= 4'd0; tr_wait <= 4'd0;
            tr_addr <= 10'd0; tr_w <= 32'd0; tr_d <= 8'd0;
        end else if (!tr_mode) begin
            tr_v <= 1'b0;
            if (trd_s[2] && !tr_fin && !svalid && tready) begin
                tr_mode <= 1'b1; tr_addr <= 10'd0; tr_wait <= 4'd15;
            end
        end else if (tr_v) begin
            if (tready) tr_v <= 1'b0;
        end else if (tr_wait != 4'd0) begin
            tr_wait <= tr_wait - 4'd1;
            if (tr_wait == 4'd1) begin tr_w <= tr_q; tr_ci <= 4'd0; end
        end else if (tr_ci < 4'd8) begin
            tr_d <= hx(tr_w[31:28]); tr_w <= {tr_w[27:0], 4'd0}; tr_v <= 1'b1; tr_ci <= tr_ci + 4'd1;
        end else begin
            tr_d <= 8'h0A; tr_v <= 1'b1;
            if (tr_addr == 10'd1023) begin tr_mode <= 1'b0; tr_fin <= 1'b1; end
            else begin tr_addr <= tr_addr + 10'd1; tr_wait <= 4'd15; end
        end
    end
    wire [7:0] tdata  = tr_mode ? tr_d : sdata;
    wire       tvalid = tr_mode ? tr_v : svalid;

    // ------------------------------------------------ LED blink code
    wire [3:0] upulses = !usb_dev_seen ? 4'd1 :
                         (key_seen    ? 4'd5 :
                         (usb_polling ? 4'd4 :
                         (any_rx      ? 4'd3 : 4'd2)));
    wire [3:0] ucycle  = (upulses << 1) + 4'd4;
    reg [22:0] ucnt;
    reg [3:0]  uslot;
    always @(posedge clk_27m or negedge clk27_rst_n)
        if (!clk27_rst_n) begin ucnt <= 23'd0; uslot <= 4'd0; end
        else if (ucnt == 23'h5FFFFF) begin
            ucnt  <= 23'd0;
            uslot <= ((uslot + 4'd1) >= ucycle) ? 4'd0 : (uslot + 4'd1);
        end else ucnt <= ucnt + 23'd1;
    assign led[0] = 1'b0;
    assign led[1] = 1'b0;
    assign led[2] = 1'b0;
    assign led[3] = (uslot < (upulses << 1)) && !uslot[0];
    wire _unused = &{1'b0, cdc_key, cdc_mod, tr_q, tr_done};
endmodule
