// ============================================================================
// Palette - 16-colour CGA/ANSI palette (matches the z80neo protocol)
// ============================================================================
//
// Index order matches sdl_client/src/render.c and the VoidVDP terminal:
//   0 black   1 blue    2 green   3 cyan
//   4 red     5 magenta 6 brown   7 light grey
//   8 dgrey   9 bblue  10 bgreen 11 bcyan
//  12 bred   13 bmagenta 14 yellow 15 white
//
// Combinational lookup; the render pipeline registers its result.

module palette (
    input      [3:0] index,
    output reg [7:0] red,
    output reg [7:0] green,
    output reg [7:0] blue
);

    always @(*) begin
        case (index)
            4'h0: begin red = 8'h00; green = 8'h00; blue = 8'h00; end
            4'h1: begin red = 8'h00; green = 8'h00; blue = 8'hAA; end
            4'h2: begin red = 8'h00; green = 8'hAA; blue = 8'h00; end
            4'h3: begin red = 8'h00; green = 8'hAA; blue = 8'hAA; end
            4'h4: begin red = 8'hAA; green = 8'h00; blue = 8'h00; end
            4'h5: begin red = 8'hAA; green = 8'h00; blue = 8'hAA; end
            4'h6: begin red = 8'hAA; green = 8'h55; blue = 8'h00; end
            4'h7: begin red = 8'hAA; green = 8'hAA; blue = 8'hAA; end
            4'h8: begin red = 8'h55; green = 8'h55; blue = 8'h55; end
            4'h9: begin red = 8'h55; green = 8'h55; blue = 8'hFF; end
            4'hA: begin red = 8'h55; green = 8'hFF; blue = 8'h55; end
            4'hB: begin red = 8'h55; green = 8'hFF; blue = 8'hFF; end
            4'hC: begin red = 8'hFF; green = 8'h55; blue = 8'h55; end
            4'hD: begin red = 8'hFF; green = 8'h55; blue = 8'hFF; end
            4'hE: begin red = 8'hFF; green = 8'hFF; blue = 8'h55; end
            4'hF: begin red = 8'hFF; green = 8'hFF; blue = 8'hFF; end
        endcase
    end

endmodule
