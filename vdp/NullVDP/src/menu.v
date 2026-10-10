// ============================================================================
// menu.v - F12 options menu overlay (writes the overlay RAM only)
// ============================================================================
//
// The menu paints a bordered box into overlay_ram, which the text controller
// muxes over the box area while `active` is high.  Closing drops `active`, so
// the terminal content underneath reappears untouched.
//
// Navigation: Up/Down move the selection, Left/Right change a value, Enter
// runs the selected action or opens an info page, Esc backs out / closes, F12
// opens / closes.  While the menu is open the top module consumes the keyboard
// locally (nothing is forwarded to the z80neo).
//
// Settings live in this module and are exported as cursor_en / local_echo /
// pal_bright; actions are the one-cycle pulses act_clear / act_reset.
// ============================================================================

module menu (
    input             clk,          // 27 MHz
    input             rst_n,

    input             key_toggle,
    input             key_up,
    input             key_down,
    input             key_left,
    input             key_right,
    input             key_enter,
    input             key_esc,

    output reg        active,
    output            box_open,

    // settings; local_echo: 0 = AUTO (echo when standalone), 1 = ON, 2 = OFF
    output reg        cursor_en,
    output reg [1:0]  local_echo,
    output reg        pal_bright,

    // actions (1-cycle)
    output reg        act_clear,
    output reg        act_reset,

    // overlay RAM write port
    output reg        ov_we,
    output reg [9:0]  ov_addr,
    output reg [7:0]  ov_char,
    output reg [15:0] ov_attr
);

    localparam [6:0] X0 = 7'd20;
    localparam [5:0] Y0 = 6'd8;
    localparam [6:0] W  = 7'd40;
    localparam [5:0] H  = 6'd13;

    localparam TXT_W = 38;
    localparam PAGE_STRIDE = TXT_W * 11;    // 418

    localparam [15:0] A_BORDER = 16'h001B;
    localparam [15:0] A_TITLE  = 16'h001E;
    localparam [15:0] A_BODY   = 16'h001F;
    localparam [15:0] A_HILITE = 16'h00F0;

    localparam [3:0] MAXSEL = 4'd7;

    localparam [1:0]
        S_CLOSED = 2'd0,
        S_DRAW   = 2'd1,
        S_ACTIVE = 2'd2;

    reg [1:0] state;
    reg [3:0] page;
    reg [3:0] sel;

    reg [5:0] r;
    reg [6:0] c;
    reg       phase;

    // ---- ROM address for the current cell ----------------------------------
    wire [4:0] tline_x = (r == 0) ? 5'd0 : ({1'b0, r} - 6'd1);
    wire [3:0] tline  = (tline_x > 5'd10) ? 4'd10 : tline_x[3:0];
    wire [5:0] tcol   = (c == 0) ? 6'd0 : (c[5:0] - 6'd1);
    wire [5:0] tcol_c = (tcol > 6'd37) ? 6'd37 : tcol;
    wire [11:0] rom_addr = page * PAGE_STRIDE + tline * TXT_W + tcol_c;

    wire [7:0] rom_char;
    menu_rom u_rom (.clk(clk), .addr(rom_addr), .data(rom_char));

    wire on_border = (r == 0) || (r == H - 6'd1) || (c == 0) || (c == W - 7'd1);

    // ---- dynamic value strings ---------------------------------------------
    function [7:0] v_onoff;
        input       en;
        input [5:0] i;
        begin
            if (en) v_onoff = (i == 0) ? 8'h4F : (i == 1) ? 8'h4E : 8'h20;   // "ON"
            else    v_onoff = (i == 0) ? 8'h4F : (i == 1) ? 8'h46 :
                              (i == 2) ? 8'h46 : 8'h20;                       // "OFF"
        end
    endfunction

    function [7:0] v_echo;
        input [1:0] m;
        input [5:0] i;
        begin
            case (m)
            2'd0: v_echo = (i == 0) ? 8'h41 : (i == 1) ? 8'h55 : (i == 2) ? 8'h54 :
                           (i == 3) ? 8'h4F : 8'h20;                             // AUTO
            2'd1: v_echo = (i == 0) ? 8'h4F : (i == 1) ? 8'h4E : 8'h20;          // ON
            default: v_echo = (i == 0) ? 8'h4F : (i == 1) ? 8'h46 :
                              (i == 2) ? 8'h46 : 8'h20;                          // OFF
            endcase
        end
    endfunction

    function [7:0] v_pal;
        input       br;
        input [5:0] i;
        begin
            if (br) v_pal = (i == 0) ? 8'h42 : (i == 1) ? 8'h52 : (i == 2) ? 8'h49 :
                            (i == 3) ? 8'h47 : (i == 4) ? 8'h48 : (i == 5) ? 8'h54 : 8'h20; // BRIGHT
            else    v_pal = (i == 0) ? 8'h4E : (i == 1) ? 8'h4F : (i == 2) ? 8'h52 :
                            (i == 3) ? 8'h4D : (i == 4) ? 8'h41 : (i == 5) ? 8'h4C : 8'h20; // NORMAL
        end
    endfunction

    reg [7:0] body_ch;
    always @(*) begin
        if (page == 4'd0 && r == 6'd2 && c >= 7'd20)      body_ch = v_onoff(cursor_en, c[5:0] - 6'd20);
        else if (page == 4'd0 && r == 6'd3 && c >= 7'd20) body_ch = v_echo(local_echo, c[5:0] - 6'd20);
        else if (page == 4'd0 && r == 6'd4 && c >= 7'd20) body_ch = v_pal(pal_bright, c[5:0] - 6'd20);
        else body_ch = (rom_char == 8'h00) ? 8'h20 : rom_char;
    end

    reg [7:0]  wchar;
    reg [15:0] wattr;
    always @(*) begin
        if (on_border) begin
            if ((r == 0 || r == H - 6'd1) && (c == 0 || c == W - 7'd1))
                wchar = 8'h2B;
            else if (r == 0 || r == H - 6'd1)
                wchar = 8'h2D;
            else
                wchar = 8'h7C;
            wattr = A_BORDER;
        end else if (r == 6'd1) begin
            wchar = rom_char;
            wattr = A_TITLE;
        end else begin
            wchar = body_ch;
            if (page == 4'd0 && r >= 6'd2 && r <= 6'd9 && (r - 6'd2) == {2'b00, sel})
                wattr = A_HILITE;
            else
                wattr = A_BODY;
        end
    end

    assign box_open = (state != S_CLOSED);

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            state <= S_CLOSED;
            page <= 4'd0; sel <= 4'd0;
            r <= 6'd0; c <= 7'd0; phase <= 1'b0;
            active <= 1'b0; ov_we <= 1'b0;
            ov_addr <= 10'd0; ov_char <= 8'h20; ov_attr <= A_BODY;
            cursor_en <= 1'b1; local_echo <= 2'd0; pal_bright <= 1'b0;
            act_clear <= 1'b0; act_reset <= 1'b0;
        end else begin
            ov_we     <= 1'b0;
            act_clear <= 1'b0;
            act_reset <= 1'b0;
            active    <= (state == S_ACTIVE);
            ov_addr   <= r * W + c;

            case (state)
            S_CLOSED: begin
                r <= 6'd0; c <= 7'd0; phase <= 1'b0;
                if (key_toggle) begin
                    page <= 4'd0; sel <= 4'd0;
                    state <= S_DRAW;
                end
            end

            S_DRAW: begin
                if (!phase) begin
                    phase <= 1'b1;              // rom_addr presented
                end else begin
                    phase <= 1'b0;
                    ov_we   <= 1'b1;
                    ov_char <= wchar;
                    ov_attr <= wattr;
                    if (c == W - 7'd1) begin
                        c <= 7'd0;
                        if (r == H - 6'd1) begin
                            r <= 6'd0; state <= S_ACTIVE;
                        end else r <= r + 6'd1;
                    end else c <= c + 7'd1;
                end
            end

            S_ACTIVE: begin
                if (key_toggle) begin
                    state <= S_CLOSED;
                end else if (key_esc) begin
                    if (page == 4'd0) state <= S_CLOSED;
                    else begin page <= 4'd0; r <= 6'd0; c <= 7'd0; phase <= 1'b0; state <= S_DRAW; end
                end else if (page == 4'd0) begin
                    if (key_up) begin
                        sel <= (sel == 4'd0) ? MAXSEL : sel - 4'd1;
                        r <= 6'd0; c <= 7'd0; phase <= 1'b0; state <= S_DRAW;
                    end else if (key_down) begin
                        sel <= (sel == MAXSEL) ? 4'd0 : sel + 4'd1;
                        r <= 6'd0; c <= 7'd0; phase <= 1'b0; state <= S_DRAW;
                    end else if (key_left || key_right) begin
                        if (sel == 4'd0) cursor_en  <= ~cursor_en;
                        if (sel == 4'd1) local_echo <= (local_echo == 2'd2) ? 2'd0 : local_echo + 2'd1;
                        if (sel == 4'd2) pal_bright <= ~pal_bright;
                        r <= 6'd0; c <= 7'd0; phase <= 1'b0; state <= S_DRAW;
                    end else if (key_enter) begin
                        case (sel)
                        4'd3: act_clear <= 1'b1;
                        4'd4: act_reset <= 1'b1;
                        4'd5: begin page <= 4'd1; r <= 6'd0; c <= 7'd0; phase <= 1'b0; state <= S_DRAW; end
                        4'd6: begin page <= 4'd2; r <= 6'd0; c <= 7'd0; phase <= 1'b0; state <= S_DRAW; end
                        4'd7: state <= S_CLOSED;
                        default: ;
                        endcase
                    end
                end else begin
                    if (key_enter) begin page <= 4'd0; r <= 6'd0; c <= 7'd0; phase <= 1'b0; state <= S_DRAW; end
                end
            end

            default: state <= S_CLOSED;
            endcase
        end
    end

endmodule
