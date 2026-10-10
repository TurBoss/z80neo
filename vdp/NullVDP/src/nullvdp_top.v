// ============================================================================
// NullVDP - Top module (SPI + HDMI + PS/2 keyboard)
//
// USB OTG (ULPI) is intentionally NOT instantiated: the USB3317 ULPI pins share
// FPGA IO banks 0/1 with the HDMI TMDS pins, which the open-source Gowin flow
// cannot mix, so adding the ULPI host kills the HDMI output.  PS/2 lives in
// bank 2/3 and coexists with HDMI.
//
// The bring-up UART console was removed; the console arrives over SPI.
// ============================================================================
module nullvdp_top (
    input clk_27m, input rst_n,
    input ps2_clk, input ps2_data,
    input spi_sck, input spi_mosi, input spi_cs_n, output spi_miso,
    output [3:0] led,
    output tmds_clk_p, output tmds_clk_n,
    output [2:0] tmds_data_p, output [2:0] tmds_data_n
);
    wire serial_clk, pix_clk, pll_lock, hdmi_rst_n;
    hdmi_pll u_pll (.clk_27m(clk_27m), .rst_n(rst_n), .serial_clk(serial_clk),
                    .pix_clk(pix_clk), .pll_lock(pll_lock), .hdmi_rst_n(hdmi_rst_n));

    reg [7:0] por_cnt;
    always @(posedge clk_27m) if (por_cnt != 8'hFF) por_cnt <= por_cnt + 8'd1;
    wire clk27_rst_n = rst_n & (por_cnt == 8'hFF);
    assign led[0]=pll_lock; assign led[2]=menu_active | gfx_active_sys; assign led[3]=hdmi_rst_n;

    wire [11:0] ca, aa, cwa, awa;
    wire [7:0]  cd, cwd;
    wire [15:0] ad, awd;
    wire cwe, awe;
    wire        k_toggle, k_up, k_down, k_enter, k_esc;
    wire [7:0]  ps2_code;
    wire        ps2_code_valid, ps2_code_break, ps2_code_ext;
    wire        ps2_toggle, ps2_up, ps2_down, ps2_left, ps2_right, ps2_enter, ps2_esc;
    wire [7:0]  ps2_ascii_data;
    wire        ps2_ascii_valid;
    wire        menu_active, menu_box_open;
    wire        menu_cursor_en, menu_cursor_style, menu_pal_bright, menu_bell_en;
    wire [1:0]  menu_local_echo;
    wire        menu_act_clear, menu_act_reset;
    wire        ov_we;
    wire [9:0]  ov_addr;
    wire [7:0]  ov_char;
    wire [15:0] ov_attr;
    wire [9:0]  ov_rd_addr;
    wire [7:0]  ov_rd_char;
    wire [15:0] ov_rd_attr;
    reg [23:0] uart_led_timer;
    always @(posedge clk_27m or negedge clk27_rst_n) begin
        if (!clk27_rst_n) uart_led_timer <= 24'd0;
        else if (ps2_ascii_valid || spi_active) uart_led_timer <= 24'hFFFFFF;
        else if (uart_led_timer > 0) uart_led_timer <= uart_led_timer - 24'd1;
    end
    assign led[1] = |uart_led_timer;

    // Console input is merged from two sources, highest priority first:
    //   1. SPI (the z80neo host drives the display over the SPI link)
    //   2. PS/2 keys, so they type locally on the HDMI screen
    // PS/2 bytes also fill the SPI RX FIFO so the host can read the keyboard.
    wire       th_rx_ready;
    wire [7:0] spi_tx_byte;
    wire       spi_tx_valid;
    wire       spi_active;
    // PS/2 bytes reach the host only when the menu is not open.
    wire ps2_key_in = ps2_ascii_valid && !menu_active;
    spi_slave u_spi (
        .clk(clk_27m), .rst_n(clk27_rst_n),
        .spi_sck(spi_sck), .spi_mosi(spi_mosi), .spi_cs_n(spi_cs_n),
        .spi_miso(spi_miso),
        .tx_byte(spi_tx_byte), .tx_valid(spi_tx_valid), .tx_ready(th_rx_ready),
        .spi_active(spi_active),
        .rx_byte(ps2_ascii_data), .rx_valid(ps2_key_in)
    );

    reg  [7:0] kb_hold;
    reg        kb_pend;
    always @(posedge clk_27m or negedge clk27_rst_n) begin
        if (!clk27_rst_n) begin kb_hold <= 8'd0; kb_pend <= 1'b0; end
        else if (ps2_ascii_valid) begin kb_hold <= ps2_ascii_data; kb_pend <= 1'b1; end
        else if (kb_pend && th_rx_ready && !spi_tx_valid) kb_pend <= 1'b0;
    end

    // When the z80neo host is driving the display it echoes the keyboard back,
    // so suppress the local echo to avoid doubled characters.  With no recent
    // SPI traffic the NullVDP is standalone and types locally.  The menu is
    // always consumed locally.
    reg [21:0] host_act_timer;
    always @(posedge clk_27m or negedge clk27_rst_n) begin
        if (!clk27_rst_n) host_act_timer <= 22'd0;
        else if (spi_active) host_act_timer <= 22'h3FFFFF;     // ~0.15 s
        else if (host_act_timer != 22'd0) host_act_timer <= host_act_timer - 22'd1;
    end
    wire host_active = (host_act_timer != 22'd0);
    // menu_local_echo: 0 = AUTO (echo only when no host), 1 = ON (force local
    // echo even with the host), 2 = OFF.  AUTO keeps the host's echo; ON lets
    // the user type locally even though the z80neo polls the link continuously.
    wire local_echo_on = (menu_local_echo == 2'd1) ||
                         (menu_local_echo == 2'd0 && !host_active);
    wire kb_local    = kb_pend && !menu_active && local_echo_on;

    // Menu actions inject a control sequence into the console stream
    // (Clear = ESC [ 2 J, Reset = ESC c).
    reg  [7:0] inj_buf [0:3];
    reg  [1:0] inj_idx, inj_len;
    reg        inj_active;
    always @(posedge clk_27m or negedge clk27_rst_n) begin
        if (!clk27_rst_n) begin inj_active <= 1'b0; inj_idx <= 2'd0; inj_len <= 2'd0; end
        else if (menu_act_clear) begin
            inj_buf[0] <= 8'h1B; inj_buf[1] <= 8'h5B; inj_buf[2] <= 8'h32; inj_buf[3] <= 8'h4A;
            inj_len <= 2'd3; inj_idx <= 2'd0; inj_active <= 1'b1;
        end else if (menu_act_reset) begin
            inj_buf[0] <= 8'h1B; inj_buf[1] <= 8'h63;
            inj_len <= 2'd1; inj_idx <= 2'd0; inj_active <= 1'b1;
        end else if (inj_active && th_rx_ready) begin
            if (inj_idx == inj_len) inj_active <= 1'b0;
            else inj_idx <= inj_idx + 2'd1;
        end
    end
    wire [7:0] inj_ch = inj_buf[inj_idx];

    wire [7:0] rx_byte  = inj_active ? inj_ch :
                          spi_tx_valid ? spi_tx_byte : kb_hold;
    wire       rx_valid = inj_active | spi_tx_valid | kb_local;

    wire [6:0] term_cursor_x;
    wire [5:0] term_cursor_y;
    wire       term_cursor_vis;
    wire [5:0] term_scroll_off;
    wire        gfx_active_sys;
    wire [2:0]  gfx_fg, gfx_bg;
    wire        gfx_start;
    wire [1:0]  gfx_shape;
    wire [2:0]  gfx_colour;
    wire [10:0] gfx_ax, gfx_bx;
    wire [8:0]  gfx_ay, gfx_by;
    wire        gfx_busy;
    wire [2:0]  gfx_we, gfx_wdata;
    wire [17:0] gfx_waddr;
    wire        th_bell;
    terminal_handler th (
        .clk(clk_27m), .rst_n(clk27_rst_n),
        .rx_data(rx_byte), .rx_valid(rx_valid), .rx_ready(th_rx_ready),
        .char_we(cwe), .char_waddr(cwa), .char_wdata(cwd),
        .attr_we(awe), .attr_waddr(awa), .attr_wdata(awd),
        .cursor_x(term_cursor_x), .cursor_y(term_cursor_y),
        .cursor_visible(term_cursor_vis), .scroll_off(term_scroll_off),
        .key_toggle(k_toggle), .key_up(k_up), .key_down(k_down),
        .key_enter(k_enter), .key_esc(k_esc),
        .gfx_active(gfx_active_sys), .gfx_fg(gfx_fg), .gfx_bg(gfx_bg),
        .gfx_start(gfx_start), .gfx_shape(gfx_shape), .gfx_colour(gfx_colour),
        .gfx_ax(gfx_ax), .gfx_ay(gfx_ay), .gfx_bx(gfx_bx), .gfx_by(gfx_by),
        .gfx_busy(gfx_busy), .bell(th_bell)
    );

    gfx_engine u_gfxeng (
        .clk(clk_27m), .rst_n(clk27_rst_n),
        .start(gfx_start), .shape(gfx_shape), .colour(gfx_colour),
        .ax(gfx_ax), .ay(gfx_ay), .bx(gfx_bx), .by(gfx_by),
        .busy(gfx_busy),
        .we(gfx_we), .waddr(gfx_waddr), .wdata(gfx_wdata)
    );

    ps2_host u_ps2 (
        .clk(clk_27m), .rst_n(clk27_rst_n),
        .ps2_clk(ps2_clk), .ps2_data(ps2_data),
        .scancode(ps2_code), .sc_valid(ps2_code_valid),
        .sc_break(ps2_code_break), .sc_ext(ps2_code_ext), .frame_err()
    );
    ps2_keys u_ps2keys (
        .clk(clk_27m), .rst_n(clk27_rst_n),
        .scancode(ps2_code), .sc_valid(ps2_code_valid),
        .sc_break(ps2_code_break), .sc_ext(ps2_code_ext),
        .key_toggle(ps2_toggle), .key_up(ps2_up), .key_down(ps2_down),
        .key_left(ps2_left), .key_right(ps2_right),
        .key_enter(ps2_enter), .key_esc(ps2_esc)
    );
    ps2_ascii u_ps2ascii (
        .clk(clk_27m), .rst_n(clk27_rst_n),
        .scancode(ps2_code), .sc_valid(ps2_code_valid),
        .sc_break(ps2_code_break), .sc_ext(ps2_code_ext),
        .ascii_data(ps2_ascii_data), .ascii_valid(ps2_ascii_valid)
    );

    menu u_menu (
        .clk(clk_27m), .rst_n(clk27_rst_n),
        .key_toggle(k_toggle | ps2_toggle),
        .key_up(ps2_up | (k_up & ~menu_active)),
        .key_down(ps2_down | (k_down & ~menu_active)),
        .key_left(ps2_left), .key_right(ps2_right),
        .key_enter(ps2_enter | (k_enter & ~menu_active)),
        .key_esc(ps2_esc | (k_esc & ~menu_active)),
        .active(menu_active), .box_open(menu_box_open),
        .cursor_en(menu_cursor_en), .local_echo(menu_local_echo),
        .cursor_style(menu_cursor_style), .bell_en(menu_bell_en),
        .pal_bright(menu_pal_bright),
        .act_clear(menu_act_clear), .act_reset(menu_act_reset),
        .ov_we(ov_we), .ov_addr(ov_addr), .ov_char(ov_char), .ov_attr(ov_attr)
    );

    overlay_ram u_ov (
        .clk_rd(pix_clk), .rd_addr(ov_rd_addr),
        .rd_char(ov_rd_char), .rd_attr(ov_rd_attr),
        .clk_wr(clk_27m), .wr_en(ov_we), .wr_addr(ov_addr),
        .wr_char(ov_char), .wr_attr(ov_attr)
    );

    wire [6:0] cur_x_s;
    wire [5:0] cur_y_s;
    wire       cur_vis_s;
    wire [5:0] off_s;
    sync_cursor u_sync (
        .clk_in(clk_27m), .clk_out(pix_clk),
        .rst_n_in(clk27_rst_n), .rst_n_out(hdmi_rst_n),
        .cur_x_in(term_cursor_x), .cur_y_in(term_cursor_y),
        .cur_vis_in(term_cursor_vis & menu_cursor_en), .off_in(term_scroll_off),
        .cur_x_out(cur_x_s), .cur_y_out(cur_y_s),
        .cur_vis_out(cur_vis_s), .off_out(off_s)
    );
    char_buffer cb (
        .clk_rd(pix_clk), .rd_addr(ca), .rd_data(cd),
        .clk_wr(clk_27m), .wr_en(cwe), .wr_addr(cwa), .wr_data(cwd)
    );
    attr_buffer ab (
        .clk_rd(pix_clk), .rd_addr(aa), .rd_data(ad),
        .clk_wr(clk_27m), .wr_en(awe), .wr_addr(awa), .wr_data(awd)
    );

    wire [17:0] gfx_rd_addr;
    wire [2:0]  gfx_rd_data;
    gfx_buffer u_gfx (
        .clk_rd(pix_clk), .rd_addr(gfx_rd_addr), .rd_data(gfx_rd_data),
        .clk_wr(clk_27m), .we(gfx_we), .wr_addr(gfx_waddr), .wr_data(gfx_wdata)
    );

    // bitmap-mode flag crosses from the system to the pixel clock domain
    reg gfx_act_meta, gfx_act_pix;
    always @(posedge pix_clk or negedge hdmi_rst_n) begin
        if (!hdmi_rst_n) begin gfx_act_meta <= 1'b0; gfx_act_pix <= 1'b0; end
        else begin gfx_act_meta <= gfx_active_sys; gfx_act_pix <= gfx_act_meta; end
    end

    // visual bell: stretch a one-cycle BEL pulse into a ~150 ms whole-frame flash
    reg bell_meta, bell_sync;
    reg [22:0] bell_cnt;
    always @(posedge pix_clk or negedge hdmi_rst_n) begin
        if (!hdmi_rst_n) begin
            bell_meta <= 1'b0; bell_sync <= 1'b0; bell_cnt <= 23'd0;
        end else begin
            bell_meta <= th_bell;
            bell_sync <= bell_meta;
            if (bell_sync)              bell_cnt <= 23'h7FFFFF;   // reload the flash
            else if (bell_cnt != 23'd0) bell_cnt <= bell_cnt - 23'd1;
        end
    end
    wire bell_act_pix = menu_bell_en && (bell_cnt != 23'd0);

    wire hs, vs, de;
    wire [7:0] r, g, b;
    text_controller u_tc (
        .clk(pix_clk), .rst_n(hdmi_rst_n),
        .cursor_x(cur_x_s), .cursor_y(cur_y_s), .cursor_visible(cur_vis_s), .scroll_off(off_s),
        .cursor_style(menu_cursor_style), .pal_bright(menu_pal_bright), .bell(bell_act_pix),
        .char_addr(ca), .char_data(cd), .attr_addr(aa), .attr_data(ad),
        .menu_active(menu_active),
        .ov_rd_addr(ov_rd_addr), .ov_rd_char(ov_rd_char), .ov_rd_attr(ov_rd_attr),
        .gfx_active(gfx_act_pix), .gfx_addr(gfx_rd_addr), .gfx_data(gfx_rd_data),
        .hsync(hs), .vsync(vs), .de(de), .red(r), .green(g), .blue(b)
    );

    wire [2:0] mode = de ? 3'd1 : 3'd0;
    wire [9:0] tr, tg, tb;
    tmds_channel #(.CN(0)) eb (.clk_pixel(pix_clk), .video_data(b), .data_island_data(4'd0), .control_data({vs,hs}), .mode(mode), .tmds(tb));
    tmds_channel #(.CN(1)) eg (.clk_pixel(pix_clk), .video_data(g), .data_island_data(4'd0), .control_data(2'b00), .mode(mode), .tmds(tg));
    tmds_channel #(.CN(2)) er (.clk_pixel(pix_clk), .video_data(r), .data_island_data(4'd0), .control_data(2'b00), .mode(mode), .tmds(tr));
    tmds_serializer sr (.serial_clk(serial_clk), .pix_clk(pix_clk), .rst_n(hdmi_rst_n), .data(tr), .tmds_p(tmds_data_p[2]), .tmds_n(tmds_data_n[2]));
    tmds_serializer sg (.serial_clk(serial_clk), .pix_clk(pix_clk), .rst_n(hdmi_rst_n), .data(tg), .tmds_p(tmds_data_p[1]), .tmds_n(tmds_data_n[1]));
    tmds_serializer sb (.serial_clk(serial_clk), .pix_clk(pix_clk), .rst_n(hdmi_rst_n), .data(tb), .tmds_p(tmds_data_p[0]), .tmds_n(tmds_data_n[0]));
    tmds_clock_serializer sc (.serial_clk(serial_clk), .pix_clk(pix_clk), .rst_n(hdmi_rst_n), .tmds_clk_p(tmds_clk_p), .tmds_clk_n(tmds_clk_n));
endmodule
