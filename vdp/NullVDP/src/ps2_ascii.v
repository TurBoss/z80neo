// ============================================================================
// ps2_ascii - scan code set 2 -> ASCII (US layout) for the console
//
// Tracks the shift keys (12/59) and maps make codes to ASCII so a PS/2
// keyboard can type into the z80neo console.  Release codes and extended
// (E0-prefixed) keys produce nothing here - the arrows/function keys are
// handled by ps2_keys for the local menu.
// ============================================================================
module ps2_ascii (
    input        clk,
    input        rst_n,
    input  [7:0] scancode,
    input        sc_valid,
    input        sc_break,
    input        sc_ext,
    output reg [7:0] ascii_data,
    output reg       ascii_valid
);
    reg shift;

    reg [7:0] c;
    always @(*) begin
        case (scancode)
        8'h1C: c = shift ? 8'h41 : 8'h61;  // A
        8'h32: c = shift ? 8'h42 : 8'h62;  // B
        8'h21: c = shift ? 8'h43 : 8'h63;  // C
        8'h23: c = shift ? 8'h44 : 8'h64;  // D
        8'h24: c = shift ? 8'h45 : 8'h65;  // E
        8'h2B: c = shift ? 8'h46 : 8'h66;  // F
        8'h34: c = shift ? 8'h47 : 8'h67;  // G
        8'h33: c = shift ? 8'h48 : 8'h68;  // H
        8'h43: c = shift ? 8'h49 : 8'h69;  // I
        8'h3B: c = shift ? 8'h4A : 8'h6A;  // J
        8'h42: c = shift ? 8'h4B : 8'h6B;  // K
        8'h4B: c = shift ? 8'h4C : 8'h6C;  // L
        8'h3A: c = shift ? 8'h4D : 8'h6D;  // M
        8'h31: c = shift ? 8'h4E : 8'h6E;  // N
        8'h44: c = shift ? 8'h4F : 8'h6F;  // O
        8'h4D: c = shift ? 8'h50 : 8'h70;  // P
        8'h15: c = shift ? 8'h51 : 8'h71;  // Q
        8'h2D: c = shift ? 8'h52 : 8'h72;  // R
        8'h1B: c = shift ? 8'h53 : 8'h73;  // S
        8'h2C: c = shift ? 8'h54 : 8'h74;  // T
        8'h3C: c = shift ? 8'h55 : 8'h75;  // U
        8'h2A: c = shift ? 8'h56 : 8'h76;  // V
        8'h1D: c = shift ? 8'h57 : 8'h77;  // W
        8'h22: c = shift ? 8'h58 : 8'h78;  // X
        8'h35: c = shift ? 8'h59 : 8'h79;  // Y
        8'h1A: c = shift ? 8'h5A : 8'h7A;  // Z
        8'h16: c = shift ? 8'h21 : 8'h31;  // 1 !
        8'h1E: c = shift ? 8'h40 : 8'h32;  // 2 @
        8'h26: c = shift ? 8'h23 : 8'h33;  // 3 #
        8'h25: c = shift ? 8'h24 : 8'h34;  // 4 $
        8'h2E: c = shift ? 8'h25 : 8'h35;  // 5 %
        8'h36: c = shift ? 8'h5E : 8'h36;  // 6 ^
        8'h3D: c = shift ? 8'h26 : 8'h37;  // 7 &
        8'h3E: c = shift ? 8'h2A : 8'h38;  // 8 *
        8'h46: c = shift ? 8'h28 : 8'h39;  // 9 (
        8'h45: c = shift ? 8'h29 : 8'h30;  // 0 )
        8'h4E: c = shift ? 8'h5F : 8'h2D;  // - _
        8'h55: c = shift ? 8'h2B : 8'h3D;  // = +
        8'h54: c = shift ? 8'h7B : 8'h5B;  // [ {
        8'h5B: c = shift ? 8'h7D : 8'h5D;  // ] }
        8'h5D: c = shift ? 8'h7C : 8'h5C;  // \ |
        8'h4C: c = shift ? 8'h3A : 8'h3B;  // ; :
        8'h52: c = shift ? 8'h22 : 8'h27;  // ' "
        8'h41: c = shift ? 8'h3C : 8'h2C;  // , <
        8'h49: c = shift ? 8'h3E : 8'h2E;  // . >
        8'h4A: c = shift ? 8'h3F : 8'h2F;  // / ?
        8'h0E: c = shift ? 8'h7E : 8'h60;  // ` ~
        8'h29: c = 8'h20;                  // space
        8'h0D: c = 8'h09;                  // tab
        8'h5A: c = 8'h0D;                  // enter
        8'h66: c = 8'h08;                  // backspace
        8'h76: c = 8'h1B;                  // esc
        default: c = 8'h00;
        endcase
    end

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            shift <= 1'b0; ascii_data <= 8'd0; ascii_valid <= 1'b0;
        end else begin
            ascii_valid <= 1'b0;

            if (sc_valid) begin
                if (scancode == 8'h12 || scancode == 8'h59)
                    shift <= ~sc_break;                 // left/right shift
                else if (sc_break || sc_ext)
                    ;                                   // release / extended: no console byte
                else if (c != 8'h00) begin
                    ascii_data  <= c;
                    ascii_valid <= 1'b1;
                end
            end
        end
    end
endmodule
