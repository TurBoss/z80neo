// ============================================================================
// NullVDP terminal handler - z80neo console protocol -> text/attribute buffers
// ============================================================================
//
// Implements the text subset of doc/vdp_protocol.md used by z80neo software:
//   - printable ASCII, CR/LF/VT/FF/BS/DEL/HT, BEL
//   - ESC c reset, ESC 7/8 save/restore, ESC D/E/M index / next / reverse
//   - CSI cursor motion A B C D E F G H f d
//   - CSI erase J K (0/1/2)
//   - CSI SGR m (0 1 4 5 7 22 24 25 27; 30-37/40-47; 90-97/100-107; 39/49;
//     38;5;n and 48;5;n), processed one parameter per cycle
//   - CSI ? h/l modes (cursor visibility ?25, autowrap ?7)
//   - CSI s/u save/restore
//   - ESC ( ) * + and ESC # take one designator byte (ignored)
//   - ESC g graphics: cmd + payload consumed (drawing is a later phase)
//
// Full-screen hardware scroll: a 30-row offset rotates the buffer, so a scroll
// costs one register update plus clearing the newly exposed line.
//
// Cell attribute word: [3:0] fg, [7:4] bg, [8] underline, [9] blink,
// [10] inverse.  See attr_buffer.v.
// ============================================================================

module terminal_handler #(
    parameter COLS     = 80,
    parameter ROWS     = 30,
    parameter BUF_SIZE = 2400
) (
    input             clk,          // 27 MHz system clock
    input             rst_n,

    input      [7:0]  rx_data,      // byte from the input FIFO
    input             rx_valid,     // FIFO not empty
    output            rx_ready,     // handler can consume this cycle

    output reg        char_we,
    output reg [11:0] char_waddr,
    output reg [7:0]  char_wdata,

    output reg        attr_we,
    output reg [11:0] attr_waddr,
    output reg [15:0] attr_wdata,

    output reg [6:0]  cursor_x,
    output reg [5:0]  cursor_y,
    output reg        cursor_visible,
    output reg [5:0]  scroll_off,

    // key events for the options menu (1-cycle pulses)
    output reg        key_toggle,   // F12  (CSI 2 4 ~)
    output reg        key_up,       // CSI A
    output reg        key_down,     // CSI B
    output reg        key_enter,    // CR
    output reg        key_esc,      // lone ESC (timeout)
    output reg        bell,         // BEL received (1-cycle pulse)

    // ---- ESC g graphics -----------------------------------------------------
    output reg        gfx_active,   // bitmap layer displayed (sync to pixel clk)
    output reg [2:0]  gfx_fg,       // current foreground palette index (0-7)
    output reg [2:0]  gfx_bg,       // current background palette index (0-7)
    output reg        gfx_start,    // 1-cycle pulse to gfx_engine
    output reg [1:0]  gfx_shape,    // 0 pixel, 1 line, 2 fill
    output reg [2:0]  gfx_colour,   // colour for the plotted pixels
    output reg [10:0] gfx_ax, gfx_bx,
    output reg [8:0]  gfx_ay, gfx_by,
    input             gfx_busy      // engine running; hold the console
);

    localparam [5:0] LAST_ROW = ROWS - 1;
    localparam [6:0] LAST_COL = COLS - 1;

    // ---- parser state -------------------------------------------------------
    localparam [2:0]
        ST_GROUND  = 3'd0,
        ST_ESC     = 3'd1,
        ST_CSI     = 3'd2,
        ST_INTERM  = 3'd3,
        ST_GFX_CMD = 3'd4,
        ST_GFX_PAY = 3'd5,
        ST_GFX_EXEC= 3'd6;

    reg [2:0] state;
    reg [17:0] esc_timer;
    reg       priv;
    reg [2:0] np;                 // number of collected params
    reg [9:0] p [0:7];            // padded so guarded index arithmetic stays in range
    reg [4:0] gfx_left;

    // ---- ESC g graphics params ---------------------------------------------
    reg [7:0]  gp [0:8];          // graphics payload bytes
    reg [3:0]  gfx_pidx;          // payload index
    reg [7:0]  gfx_cmd_r;         // latched graphics command byte
    reg        gfx_wait;          // hold the console until gfx_engine is done

    // ---- multi-cycle SGR ----------------------------------------------------
    reg       sgr_active;
    reg [1:0] sgr_idx;
    reg [1:0] sgr_skip;

    // ---- cursor / colours ---------------------------------------------------
    reg [6:0] cur_x, sav_x;
    reg [5:0] cur_y, sav_y;
    reg [3:0] fg, bg, sav_fg, sav_bg;
    reg       uflag, bflag, iflag;
    reg       sav_u, sav_b, sav_i;
    reg       bold;
    reg       autowrap;
    reg [5:0] off;

    // ---- fill engine (erase / scroll clear) --------------------------------
    reg        fill_active;
    reg [5:0]  fr, fr1;
    reg [6:0]  fc, fc1;

    reg [9:0] n1, n2, m;          // scratch for CSI dispatch (assigned each cycle)

    // ---- write-address helpers ---------------------------------------------
    wire [5:0]  wy   = cur_y + off;
    wire [5:0]  wy_m = (wy >= ROWS) ? (wy - ROWS) : wy;
    wire [11:0] waddr = wy_m * COLS + cur_x;
    wire [15:0] wattr = {5'b0, iflag, bflag, uflag, bg, fg};

    function [11:0] phys;
        input [5:0] r;
        input [6:0] c;
        reg   [5:0] s;
        begin
            s = r + off;
            if (s >= ROWS) s = s - ROWS;
            phys = s * COLS + c;
        end
    endfunction

    wire [11:0] faddr = phys(fr, fc);
    wire [15:0] fattr = {5'b0, 3'b000, bg, 4'h7};  // space, current bg, fg=7

    // Map a 16-colour host value to the 8-colour (3bpp) bitmap layer.  Only 8
    // colours fit, so bright shades fold onto their base colour -- but greys
    // are kept distinct (0 black, 8 dark grey, 7 light grey, 15 white) so the
    // boot logo's grey/white strokes do not collapse to black.
    function [2:0] gfxmap;
        input [3:0] c;
        begin
            case (c)
                4'd0:                      gfxmap = 3'd0;  // black
                4'd8:                      gfxmap = 3'd1;  // dark grey
                4'd7:                      gfxmap = 3'd2;  // light grey
                4'd15:                     gfxmap = 3'd3;  // white
                4'd1,  4'd9:               gfxmap = 3'd4;  // blue
                4'd2,  4'd3, 4'd10, 4'd11: gfxmap = 3'd5;  // green / cyan
                4'd4,  4'd5, 4'd12, 4'd13: gfxmap = 3'd6;  // red / magenta
                4'd6,  4'd14:              gfxmap = 3'd7;  // brown / yellow
                default:                   gfxmap = 3'd0;
            endcase
        end
    endfunction

    // The self-timed ST_GFX_EXEC cycle dispatches a command without consuming
    // rx_data, so hold rx_ready low for it -- otherwise the byte present that
    // cycle (the next stream byte) is silently dropped, desyncing the console.
    assign rx_ready = !fill_active && !sgr_active && !gfx_wait
                      && (state != ST_GFX_EXEC);

    // ---- tasks --------------------------------------------------------------
    task start_fill;
        input [5:0] r0;
        input [6:0] c0;
        input [5:0] r1;
        input [6:0] c1;
        begin
            fill_active <= 1'b1;
            fr  <= r0;
            fc  <= c0;
            fr1 <= r1;
            fc1 <= c1;
        end
    endtask

    task do_index;               // cursor down, scroll at the bottom
        begin
            if (cur_y < LAST_ROW)
                cur_y <= cur_y + 6'd1;
            else begin
                off <= (off == LAST_ROW) ? 6'd0 : off + 6'd1;
                start_fill(LAST_ROW, 7'd0, LAST_ROW, LAST_COL);
            end
        end
    endtask

    task do_rev_index;           // cursor up, scroll at the top
        begin
            if (cur_y > 0)
                cur_y <= cur_y - 6'd1;
            else begin
                off <= (off == 0) ? LAST_ROW : off - 6'd1;
                start_fill(6'd0, 7'd0, 6'd0, LAST_COL);
            end
        end
    endtask

    integer k;

    // ========================================================================
    // Main sequential logic
    // ========================================================================
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            state    <= ST_GROUND;
            priv     <= 1'b0;
            np       <= 3'd0;
            gfx_left <= 5'd0;
            gfx_pidx <= 4'd0;
            gfx_cmd_r <= 8'd0;
            gfx_wait <= 1'b0;
            gfx_active <= 1'b0;
            gfx_fg <= 3'd2;
            gfx_bg <= 3'd0;
            gfx_start <= 1'b0;
            gfx_shape <= 2'd0;
            gfx_colour <= 3'd0;
            gfx_ax <= 11'd0; gfx_bx <= 11'd0;
            gfx_ay <= 9'd0;  gfx_by <= 9'd0;
            for (k = 0; k < 9; k = k + 1) gp[k] <= 8'd0;
            sgr_active <= 1'b0;
            sgr_idx  <= 2'd0;
            sgr_skip <= 2'd0;
            for (k = 0; k < 4; k = k + 1) p[k] <= 10'd0;

            cur_x <= 7'd0;  cur_y <= 6'd0;
            sav_x <= 7'd0;  sav_y <= 6'd0;
            fg <= 4'h7;     bg <= 4'h0;
            sav_fg <= 4'h7; sav_bg <= 4'h0;
            uflag <= 1'b0;  bflag <= 1'b0;  iflag <= 1'b0;
            sav_u <= 1'b0;  sav_b <= 1'b0;  sav_i <= 1'b0;
            bold <= 1'b0;   autowrap <= 1'b1;  off <= 6'd0;

            char_we    <= 1'b0;  char_waddr <= 11'd0;  char_wdata <= 8'h20;
            attr_we    <= 1'b0;  attr_waddr <= 11'd0;  attr_wdata <= 16'h0070;

            cursor_x <= 7'd0;  cursor_y <= 6'd0;
            cursor_visible <= 1'b1;  scroll_off <= 6'd0;

            // boot: clear the whole screen with the default colours
            fill_active <= 1'b1;
            fr <= 6'd0;  fc <= 7'd0;  fr1 <= LAST_ROW;  fc1 <= LAST_COL;
        end else begin
            char_we <= 1'b0;
            attr_we <= 1'b0;
            key_toggle <= 1'b0;
            key_up <= 1'b0;
            key_down <= 1'b0;
            key_enter <= 1'b0;
            key_esc <= 1'b0;
            bell <= 1'b0;
            gfx_start <= 1'b0;
            n1 = 10'd0;
            n2 = 10'd0;
            m  = 10'd0;

            // release the console once the graphics engine has finished
            if (gfx_wait && !gfx_busy) gfx_wait <= 1'b0;

            // -----------------------------------------------------------------
            // Fill engine: write priority, blocks byte consumption
            // -----------------------------------------------------------------
            if (fill_active) begin
                char_we    <= 1'b1;
                char_waddr <= faddr;
                char_wdata <= 8'h20;
                attr_we    <= 1'b1;
                attr_waddr <= faddr;
                attr_wdata <= fattr;

                if (fc == fc1 && fr == fr1)
                    fill_active <= 1'b0;
                else if (fc == fc1) begin
                    fc <= 7'd0;
                    fr <= fr + 6'd1;
                end else
                    fc <= fc + 7'd1;
            end

            // -----------------------------------------------------------------
            // Multi-cycle SGR: one parameter per cycle
            // -----------------------------------------------------------------
            else if (sgr_active) begin
                if (sgr_skip != 2'd0) begin
                    sgr_skip <= sgr_skip - 2'd1;
                end else if ((p[sgr_idx] == 10'd38 || p[sgr_idx] == 10'd48) &&
                             ({1'b0, sgr_idx} + 3'd1 < {1'b0, np}) &&
                             (p[sgr_idx + 3'd1] == 10'd5) &&
                             ({1'b0, sgr_idx} + 3'd2 < {1'b0, np})) begin
                    if (p[sgr_idx] == 10'd38) fg <= p[sgr_idx + 3'd2][3:0];
                    else                      bg <= p[sgr_idx + 3'd2][3:0];
                    sgr_skip <= 2'd2;
                end else begin
                    case (p[sgr_idx])
                        10'd0:  begin fg<=4'h7; bg<=4'h0; bold<=1'b0;
                                      uflag<=1'b0; bflag<=1'b0; iflag<=1'b0; end
                        10'd1:  begin bold<=1'b1; fg<={1'b1, fg[2:0]}; end
                        10'd4:  uflag <= 1'b1;
                        10'd5:  bflag <= 1'b1;
                        10'd7:  iflag <= 1'b1;
                        10'd22: begin bold<=1'b0; fg<={1'b0, fg[2:0]}; end
                        10'd24: uflag <= 1'b0;
                        10'd25: bflag <= 1'b0;
                        10'd27: iflag <= 1'b0;
                        10'd39: fg <= {bold, 3'h7};
                        10'd49: bg <= 4'h0;
                        default: begin
                            if (p[sgr_idx] >= 10'd30 && p[sgr_idx] <= 10'd37) begin
                                m = p[sgr_idx] - 10'd30;
                                fg <= {bold, m[2:0]};
                            end else if (p[sgr_idx] >= 10'd40 && p[sgr_idx] <= 10'd47) begin
                                m = p[sgr_idx] - 10'd40;
                                bg <= m[3:0];
                            end else if (p[sgr_idx] >= 10'd90 && p[sgr_idx] <= 10'd97) begin
                                m = p[sgr_idx] - 10'd90;
                                fg <= {1'b1, m[2:0]};
                            end else if (p[sgr_idx] >= 10'd100 && p[sgr_idx] <= 10'd107) begin
                                m = p[sgr_idx] - 10'd100;
                                bg <= {1'b1, m[2:0]};
                            end
                        end
                    endcase
                end

                if ({1'b0, sgr_idx} + 3'd1 >= {1'b0, np})
                    sgr_active <= 1'b0;
                else
                    sgr_idx <= sgr_idx + 2'd1;
            end

            // -----------------------------------------------------------------
            // Byte consumption
            // -----------------------------------------------------------------
            // lone-ESC timeout -> menu "back/close"
            // -----------------------------------------------------------------
            // Execute a fully-collected ESC g command (self-timed, one cycle)
            // -----------------------------------------------------------------
            else if (state == ST_GFX_EXEC) begin
                state <= ST_GROUND;
                case (gfx_cmd_r)
                    8'h01: begin                            // mode on/off
                        gfx_active <= (gp[0] != 0);
                        if (gp[0] != 0) begin            // turning on clears to bg
                            gfx_start  <= 1'b1;
                            gfx_shape  <= 2'd2;
                            gfx_colour <= gfx_bg;
                            gfx_ax <= 11'd0;  gfx_ay <= 9'd0;
                            gfx_bx <= 11'd640; gfx_by <= 9'd256;
                            gfx_wait   <= 1'b1;
                        end
                    end
                    8'h02: begin                            // clear bitmap
                        gfx_start  <= 1'b1;
                        gfx_shape  <= 2'd2;
                        gfx_colour <= gfx_bg;
                        gfx_ax <= 11'd0;  gfx_ay <= 9'd0;
                        gfx_bx <= 11'd640; gfx_by <= 9'd256;
                        gfx_wait   <= 1'b1;
                    end
                        8'h03: begin                            // set fg/bg colours
                            gfx_fg <= gfxmap(gp[0][3:0]);
                            gfx_bg <= gfxmap(gp[1][3:0]);
                        end
                    8'h04: begin                            // plot pixel (fg/bg)
                        gfx_start  <= 1'b1;
                        gfx_shape  <= 2'd0;
                        gfx_colour <= gp[4] ? gfx_fg : gfx_bg;
                        gfx_ax     <= gp[0] | (gp[1] << 8);
                        gfx_ay     <= gp[2] | (gp[3] << 8);
                        gfx_wait   <= 1'b1;
                    end
                        8'h07: begin                            // plot pixel (explicit colour)
                            gfx_start  <= 1'b1;
                            gfx_shape  <= 2'd0;
                            gfx_colour <= gfxmap(gp[4][3:0]);
                        gfx_ax     <= gp[0] | (gp[1] << 8);
                        gfx_ay     <= gp[2] | (gp[3] << 8);
                        gfx_wait   <= 1'b1;
                    end
                    8'h05: begin                            // line
                        gfx_start  <= 1'b1;
                        gfx_shape  <= 2'd1;
                        gfx_colour <= gp[8] ? gfx_fg : gfx_bg;
                        gfx_ax     <= gp[0] | (gp[1] << 8);
                        gfx_ay     <= gp[2] | (gp[3] << 8);
                        gfx_bx     <= gp[4] | (gp[5] << 8);
                        gfx_by     <= gp[6] | (gp[7] << 8);
                        gfx_wait   <= 1'b1;
                    end
                    8'h06: begin                            // filled rectangle
                        gfx_start  <= 1'b1;
                        gfx_shape  <= 2'd2;
                        gfx_colour <= gp[8] ? gfx_fg : gfx_bg;
                        gfx_ax     <= gp[0] | (gp[1] << 8);
                        gfx_ay     <= gp[2] | (gp[3] << 8);
                        gfx_bx     <= gp[4] | (gp[5] << 8);
                        gfx_by     <= gp[6] | (gp[7] << 8);
                        gfx_wait   <= 1'b1;
                    end
                    default: ;
                endcase
            end

            else if (state == ST_ESC && !rx_valid) begin
                esc_timer <= esc_timer + 18'd1;
                if (esc_timer == 18'h2FFFF) begin
                    key_esc <= 1'b1;
                    state   <= ST_GROUND;
                end
            end
            // Consume only when rx_ready: the source (SPI slave) holds the byte
            // until rx_ready, so an ungated rx_valid would be re-processed every
            // cycle while the handler is busy (e.g. gfx_wait during a draw).
            else if (rx_valid && rx_ready) begin
                esc_timer <= 18'd0;
                case (state)

                // -------------------------------------------------------------
                ST_GROUND: begin
                    case (rx_data)
                        8'h1B: begin state <= ST_ESC; priv <= 1'b0; np <= 3'd0; end
                        8'h0D: begin cur_x <= 7'd0; key_enter <= 1'b1; end      // CR
                        8'h0A, 8'h0B, 8'h0C: do_index;                       // LF/VT/FF
                        8'h08, 8'h7F: if (cur_x > 0) cur_x <= cur_x - 7'd1;  // BS/DEL
                        8'h09: if (cur_x < LAST_COL)                         // HT
                                  cur_x <= {cur_x[6:3] + 4'd1, 3'd0};
                        8'h07: bell <= 1'b1;                                 // BEL
                        default: begin
                            if (rx_data >= 8'h20 && rx_data <= 8'h7E) begin
                                char_we    <= 1'b1;
                                char_waddr <= waddr;
                                char_wdata <= rx_data;
                                attr_we    <= 1'b1;
                                attr_waddr <= waddr;
                                attr_wdata <= wattr;

                                if (cur_x < LAST_COL)
                                    cur_x <= cur_x + 7'd1;
                                else if (autowrap) begin
                                    cur_x <= 7'd0;
                                    do_index;
                                end
                            end
                        end
                    endcase
                end

                // -------------------------------------------------------------
                ST_ESC: begin
                    state <= ST_GROUND;
                    case (rx_data)
                        8'h5B: begin state <= ST_CSI; priv <= 1'b0; np <= 3'd0; end
                        8'h63: begin                                        // 'c' RIS
                            cur_x <= 7'd0; cur_y <= 6'd0;
                            fg <= 4'h7; bg <= 4'h0;
                            uflag <= 1'b0; bflag <= 1'b0; iflag <= 1'b0; bold <= 1'b0;
                            autowrap <= 1'b1; off <= 6'd0;
                            start_fill(6'd0, 7'd0, LAST_ROW, LAST_COL);
                        end
                        8'h37: begin                                        // '7'
                            sav_x <= cur_x; sav_y <= cur_y;
                            sav_fg <= fg; sav_bg <= bg;
                            sav_u <= uflag; sav_b <= bflag; sav_i <= iflag;
                        end
                        8'h38: begin                                        // '8'
                            cur_x <= sav_x; cur_y <= sav_y;
                            fg <= sav_fg; bg <= sav_bg;
                            uflag <= sav_u; bflag <= sav_b; iflag <= sav_i;
                        end
                        8'h44: do_index;                                    // 'D'
                        8'h45: begin cur_x <= 7'd0; do_index; end            // 'E'
                        8'h4D: do_rev_index;                                // 'M'
                        8'h28, 8'h29, 8'h2A, 8'h2B, 8'h23: state <= ST_INTERM;
                        8'h67: state <= ST_GFX_CMD;                         // 'g'
                        default: ;
                    endcase
                end

                // -------------------------------------------------------------
                ST_INTERM: state <= ST_GROUND;   // discard the designator byte

                // -------------------------------------------------------------
                // CSI: parameter collection + single dispatch site
                // -------------------------------------------------------------
                ST_CSI: begin
                    if (rx_data == 8'h3F) begin
                        priv <= 1'b1;                                // '?'
                    end else if (rx_data >= 8'h30 && rx_data <= 8'h39) begin
                        if (np == 3'd0) begin
                            p[0] <= {8'b0, rx_data[3:0]} - 8'd0;     // digit 0-9
                            np   <= 3'd1;
                        end else begin
                            if (p[np-3'd1] > 10'd99)
                                p[np-3'd1] <= 10'd999;
                            else
                                p[np-3'd1] <= p[np-3'd1] * 10 + {8'b0, rx_data[3:0]};
                        end
                    end else if (rx_data == 8'h3B) begin             // ';'
                        if (np == 3'd0) begin
                            p[0] <= 10'd0; p[1] <= 10'd0; np <= 3'd2;
                        end else if (np < 3'd4) begin
                            np <= np + 3'd1;
                            p[np] <= 10'd0;
                        end
                    end else begin
                        n1 = (np >= 3'd1) ? p[0] : 10'd0;
                        n2 = (np >= 3'd2) ? p[1] : 10'd0;
                        state <= ST_GROUND;

                        case (rx_data)
                        8'h7E: if (n1 == 10'd24) key_toggle <= 1'b1;   // F12
                        8'h41: begin                                  // A up
                            key_up <= 1'b1;
                            m = (n1 == 0) ? 10'd1 : n1;
                            if (m >= {4'b0, cur_y}) cur_y <= 6'd0;
                            else cur_y <= cur_y - m[5:0];
                        end
                        8'h42: begin                                  // B down
                            key_down <= 1'b1;
                            m = (n1 == 0) ? 10'd1 : n1;
                            if (n1 > {4'b0, LAST_ROW} - {4'b0, cur_y}) cur_y <= LAST_ROW;
                            else cur_y <= cur_y + m[5:0];
                        end
                        8'h43: begin                                  // C right
                            m = (n1 == 0) ? 10'd1 : n1;
                            if (n1 > {3'b0, LAST_COL} - {3'b0, cur_x}) cur_x <= LAST_COL;
                            else cur_x <= cur_x + m[6:0];
                        end
                        8'h44: begin                                  // D left
                            m = (n1 == 0) ? 10'd1 : n1;
                            if (m >= {3'b0, cur_x}) cur_x <= 7'd0;
                            else cur_x <= cur_x - m[6:0];
                        end
                        8'h45: begin cur_x <= 7'd0; do_index; end     // E next line
                        8'h46: begin                                  // F prev line
                            cur_x <= 7'd0;
                            m = (n1 == 0) ? 10'd1 : n1;
                            if (m >= {4'b0, cur_y}) cur_y <= 6'd0;
                            else cur_y <= cur_y - m[5:0];
                        end
                        8'h47: begin                                  // G column abs
                            m = (n1 == 0) ? 10'd1 : n1;
                            if (m > 10'd80) cur_x <= LAST_COL;
                            else cur_x <= m[6:0] - 7'd1;
                        end
                        8'h64: begin                                  // d row abs
                            m = (n1 == 0) ? 10'd1 : n1;
                            if (m > 10'd30) cur_y <= LAST_ROW;
                            else cur_y <= m[5:0] - 6'd1;
                        end
                        8'h48, 8'h66: begin                           // H / f
                            m = (n1 == 0) ? 10'd1 : n1;
                            if (m > 10'd30) cur_y <= LAST_ROW; else cur_y <= m[5:0] - 6'd1;
                            m = (n2 == 0) ? 10'd1 : n2;
                            if (m > 10'd80) cur_x <= LAST_COL; else cur_x <= m[6:0] - 7'd1;
                        end
                        8'h4A: begin                                  // J erase display
                            case (n1)
                                10'd0: start_fill(cur_y, cur_x, LAST_ROW, LAST_COL);
                                10'd1: start_fill(6'd0, 7'd0, cur_y, cur_x);
                                default: start_fill(6'd0, 7'd0, LAST_ROW, LAST_COL);
                            endcase
                        end
                        8'h4B: begin                                  // K erase line
                            case (n1)
                                10'd0: start_fill(cur_y, cur_x, cur_y, LAST_COL);
                                10'd1: start_fill(cur_y, 7'd0, cur_y, cur_x);
                                default: start_fill(cur_y, 7'd0, cur_y, LAST_COL);
                            endcase
                        end
                        8'h6D: begin                                  // m SGR
                            if (np == 3'd0) begin
                                fg <= 4'h7; bg <= 4'h0; bold <= 1'b0;
                                uflag <= 1'b0; bflag <= 1'b0; iflag <= 1'b0;
                            end else begin
                                sgr_idx  <= 2'd0;
                                sgr_skip <= 2'd0;
                                sgr_active <= 1'b1;
                            end
                        end
                        8'h68: if (priv) begin                        // h set mode
                            if (n1 == 10'd25) cursor_visible <= 1'b1;
                            else if (n1 == 10'd7) autowrap <= 1'b1;
                        end
                        8'h6C: if (priv) begin                        // l reset mode
                            if (n1 == 10'd25) cursor_visible <= 1'b0;
                            else if (n1 == 10'd7) autowrap <= 1'b0;
                        end
                        8'h73: begin                                  // s save
                            sav_x <= cur_x; sav_y <= cur_y;
                            sav_fg <= fg; sav_bg <= bg;
                            sav_u <= uflag; sav_b <= bflag; sav_i <= iflag;
                        end
                        8'h75: begin                                  // u restore
                            cur_x <= sav_x; cur_y <= sav_y;
                            fg <= sav_fg; bg <= sav_bg;
                            uflag <= sav_u; bflag <= sav_b; iflag <= sav_i;
                        end
                        default: ;                                    // ignored
                        endcase
                    end
                end

                // -------------------------------------------------------------
                // ESC g <cmd>: latch the command and its payload length
                ST_GFX_CMD: begin
                    gfx_cmd_r <= rx_data;
                    gfx_pidx  <= 4'd0;
                    case (rx_data)
                        8'h01, 8'h02: begin gfx_left <= 5'd1; state <= ST_GFX_PAY; end
                        8'h03:        begin gfx_left <= 5'd2; state <= ST_GFX_PAY; end
                        8'h04, 8'h07: begin gfx_left <= 5'd5; state <= ST_GFX_PAY; end
                        8'h05, 8'h06: begin gfx_left <= 5'd9; state <= ST_GFX_PAY; end
                        default:      state <= ST_GROUND;
                    endcase
                end

                // ---- collect the payload bytes ------------------------------
                ST_GFX_PAY: begin
                    gp[gfx_pidx] <= rx_data;
                    if (gfx_left <= 5'd1)
                        state <= ST_GFX_EXEC;
                    else begin
                        gfx_pidx <= gfx_pidx + 4'd1;
                        gfx_left <= gfx_left - 5'd1;
                    end
                end

                // graphics dispatch happens in the self-timed ST_GFX_EXEC
                // branch above; nothing to do here.

                default: state <= ST_GROUND;
                endcase
            end

            // Update registered outputs
            cursor_x   <= cur_x;
            cursor_y   <= cur_y;
            scroll_off <= off;
        end
    end

endmodule
