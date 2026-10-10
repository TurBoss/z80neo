// ============================================================================
// Text Controller - VGA timing generator + colour character rendering
// 80 columns x 30 rows = 640x480 pixels with 8x16 font
// ============================================================================
//
// Pipeline (all position/attr/font signals advance together):
//   T   : h/v counters -> character grid coords
//   T+1 : s0_* coords registered; char_buffer output (char_data) valid
//         font_rom addressed with {char_data, s0_pix_y}
//   T+2 : s1_* coords + attr registered; font_rom output valid
//   T+3 : s2_* coords + attr + font registered
//   T+4 : colour selected + palette, RGB registered out
//
// A hardware scroll offset rotates the ROWS-row buffers: displayed row r reads
// physical row (r + scroll_off) mod ROWS.
//
// VGA 640x480 @ 60Hz (25 MHz pixel clock):
//   H_TOTAL = 800  (640 visible + 16 FP + 96 SYNC + 48 BP)
//   V_TOTAL = 525  (480 visible + 10 FP + 2 SYNC + 33 BP)
// ============================================================================

module text_controller #(
    parameter COLS   = 80,
    parameter ROWS   = 30,
    parameter CHAR_W = 8,
    parameter CHAR_H = 16,
    parameter H_RES  = 640,
    parameter V_RES  = 480,
    parameter H_FP   = 16,
    parameter H_SYNC = 96,
    parameter H_BP   = 48,
    parameter V_FP   = 10,
    parameter V_SYNC = 2,
    parameter V_BP   = 33
) (
    input             clk,
    input             rst_n,

    input      [6:0]  cursor_x,
    input      [5:0]  cursor_y,
    input             cursor_visible,
    input             pal_bright,
    input      [5:0]  scroll_off,

    output     [11:0] char_addr,
    input      [7:0]  char_data,
    output     [11:0] attr_addr,
    input      [15:0] attr_data,

    // menu overlay layer
    input             menu_active,
    output     [9:0]  ov_rd_addr,
    input      [7:0]  ov_rd_char,
    input      [15:0] ov_rd_attr,

    output            hsync,
    output            vsync,
    output            de,
    output     [7:0]  red,
    output     [7:0]  green,
    output     [7:0]  blue
);

    localparam H_TOTAL = H_RES + H_FP + H_SYNC + H_BP;  // 800
    localparam V_TOTAL = V_RES + V_FP + V_SYNC + V_BP;  // 525

    reg [10:0] h_cnt;
    reg [10:0] v_cnt;

    wire raw_hsync, raw_vsync, raw_de;

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            h_cnt <= 11'd0;
            v_cnt <= 11'd0;
        end else if (h_cnt >= H_TOTAL - 1) begin
            h_cnt <= 11'd0;
            if (v_cnt >= V_TOTAL - 1) v_cnt <= 11'd0;
            else                      v_cnt <= v_cnt + 11'd1;
        end else begin
            h_cnt <= h_cnt + 11'd1;
        end
    end

    // 640x480@60 uses negative sync polarity (matches the working apicula DVI
    // example, which sets INVERT=1); the sync pulse is LOW.
    assign raw_hsync = (h_cnt >= H_SYNC);
    assign raw_vsync = (v_cnt >= V_SYNC);
    wire h_de = (h_cnt >= (H_SYNC + H_BP)) && (h_cnt < (H_SYNC + H_BP + H_RES));
    wire v_de = (v_cnt >= (V_SYNC + V_BP)) && (v_cnt < (V_SYNC + V_BP + V_RES));
    assign raw_de = h_de && v_de;

    // ---- grid coordinates ---------------------------------------------------
    wire [9:0] pixel_x = h_cnt - (H_SYNC + H_BP);
    wire [9:0] pixel_y = v_cnt - (V_SYNC + V_BP);
    wire [6:0] char_col   = pixel_x[9:3];
    wire [2:0] char_pix_x = pixel_x[2:0];
    wire [5:0] char_row   = pixel_y[9:4];
    wire [3:0] char_pix_y = pixel_y[3:0];

    wire [5:0] row_sum  = char_row + scroll_off;
    wire [5:0] phys_row = (row_sum >= ROWS) ? (row_sum - ROWS) : row_sum;

    assign char_addr = phys_row * COLS + char_col;
    assign attr_addr = char_addr;

    // ---- menu overlay -------------------------------------------------------
    localparam [6:0] BX0 = 7'd20;
    localparam [5:0] BY0 = 6'd8;
    localparam [6:0] BW  = 7'd40;
    localparam [5:0] BH  = 6'd13;

    wire in_box = menu_active && (char_col >= BX0) && (char_col < BX0 + BW) &&
                                 (char_row >= BY0) && (char_row < BY0 + BH);
    wire [11:0] ov_off = (char_row - BY0) * BW + (char_col - BX0);
    assign ov_rd_addr = in_box ? ov_off[9:0] : 10'd0;

    // ---- Stage 0: register coords ------------------------------------------
    reg [6:0] s0_col;
    reg [2:0] s0_px;
    reg [3:0] s0_py;
    reg [5:0] s0_row;
    reg       s0_de, s0_hsync, s0_vsync;
    reg       s0_inbox;

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            s0_col <= 7'd0; s0_px <= 3'd0; s0_py <= 4'd0; s0_row <= 6'd0;
            s0_de <= 1'b0; s0_hsync <= 1'b0; s0_vsync <= 1'b0; s0_inbox <= 1'b0;
        end else begin
            s0_col <= char_col; s0_px <= char_pix_x; s0_py <= char_pix_y; s0_row <= char_row;
            s0_de <= raw_de; s0_hsync <= raw_hsync; s0_vsync <= raw_vsync;
            s0_inbox <= in_box;
        end
    end

    // font address: char_data is valid this cycle for the s0 coordinate
    wire [7:0]  eff_char = s0_inbox ? ov_rd_char : char_data;
    wire [15:0] eff_attr = s0_inbox ? ov_rd_attr : attr_data;
    wire [10:0] font_addr = {eff_char[6:0], s0_py};
    wire [7:0]  font_row;

    font_rom u_font (
        .clk  (clk),
        .addr (font_addr),
        .data (font_row)
    );

    // ---- Stage 1: attr + coords --------------------------------------------
    reg [15:0] s1_attr;
    reg [6:0]  s1_col;
    reg [2:0]  s1_px;
    reg [3:0]  s1_py;
    reg [5:0]  s1_row;
    reg        s1_de, s1_hsync, s1_vsync;

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            s1_attr <= 16'h0070;
            s1_col <= 7'd0; s1_px <= 3'd0; s1_py <= 4'd0; s1_row <= 6'd0;
            s1_de <= 1'b0; s1_hsync <= 1'b0; s1_vsync <= 1'b0;
        end else begin
            s1_attr <= eff_attr;
            s1_col <= s0_col; s1_px <= s0_px; s1_py <= s0_py; s1_row <= s0_row;
            s1_de <= s0_de; s1_hsync <= s0_hsync; s1_vsync <= s0_vsync;
        end
    end

    // ---- Stage 2: font + attr + coords -------------------------------------
    reg [7:0]  s2_font;
    reg [15:0] s2_attr;
    reg [6:0]  s2_col;
    reg [2:0]  s2_px;
    reg [3:0]  s2_py;
    reg [5:0]  s2_row;
    reg        s2_de, s2_hsync, s2_vsync;

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            s2_font <= 8'd0; s2_attr <= 16'h0070;
            s2_col <= 7'd0; s2_px <= 3'd0; s2_py <= 4'd0; s2_row <= 6'd0;
            s2_de <= 1'b0; s2_hsync <= 1'b0; s2_vsync <= 1'b0;
        end else begin
            s2_font <= font_row;
            s2_attr <= s1_attr;
            s2_col <= s1_col; s2_px <= s1_px; s2_py <= s1_py; s2_row <= s1_row;
            s2_de <= s1_de; s2_hsync <= s1_hsync; s2_vsync <= s1_vsync;
        end
    end

    // ---- Stage 3: colour + palette -> registered RGB ------------------------
    reg [7:0] s3_red, s3_green, s3_blue;
    reg       s3_de, s3_hsync, s3_vsync;

    // blink ~1.5 Hz at 25 MHz
    reg [23:0] blink_cnt;
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) blink_cnt <= 24'd0;
        else        blink_cnt <= blink_cnt + 24'd1;
    end
    wire blink = blink_cnt[23];

    wire [3:0] attr_fg  = s2_attr[3:0];
    wire [3:0] attr_bg  = s2_attr[7:4];
    wire       attr_ul  = s2_attr[8];
    wire       attr_bl  = s2_attr[9];
    wire       attr_inv = s2_attr[10];

    wire [3:0] eff_fg = attr_inv ? attr_bg : attr_fg;
    wire [3:0] eff_bg = attr_inv ? attr_fg : attr_bg;

    wire pixel_on  = s2_font[7 - s2_px];
    wire hide      = attr_bl & blink;
    wire underline = attr_ul && (s2_py >= 4'd14);
    wire cur_on    = (s2_col == cursor_x) && (s2_row == cursor_y) &&
                     cursor_visible && blink;

    wire [3:0] norm_col = underline ? eff_fg :
                          ((pixel_on && !hide) ? eff_fg : eff_bg);
    wire [3:0] curs_col = pixel_on ? eff_bg : eff_fg;
    wire [3:0] pix_col  = s2_de ? (cur_on ? curs_col : norm_col) : 4'h0;

    wire [7:0] pal_r, pal_g, pal_b;
    palette u_pal (
        .index (pix_col),
        .red   (pal_r),
        .green (pal_g),
        .blue  (pal_b)
    );

    // "bright" palette mode scales the RGB by ~1.5x (keeps black black)
    wire [7:0] br_r = pal_bright ? (pal_r + (pal_r >> 1)) : pal_r;
    wire [7:0] br_g = pal_bright ? (pal_g + (pal_g >> 1)) : pal_g;
    wire [7:0] br_b = pal_bright ? (pal_b + (pal_b >> 1)) : pal_b;

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            s3_red <= 8'd0; s3_green <= 8'd0; s3_blue <= 8'd0;
            s3_de <= 1'b0; s3_hsync <= 1'b0; s3_vsync <= 1'b0;
        end else begin
            s3_red   <= br_r;
            s3_green <= br_g;
            s3_blue  <= br_b;
            s3_de    <= s2_de;
            s3_hsync <= s2_hsync;
            s3_vsync <= s2_vsync;
        end
    end

    assign red   = s3_red;
    assign green = s3_green;
    assign blue  = s3_blue;
    assign de    = s3_de;
    assign hsync = s3_hsync;
    assign vsync = s3_vsync;

endmodule
