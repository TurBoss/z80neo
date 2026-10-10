// ============================================================================
// ps2_keys - scan code set 2 -> z80neo/NullVDP key events
//
// Emits one-cycle pulses for the keys the local menu reacts to, so a PS/2
// keyboard behaves exactly like the F12/arrow/Enter/Esc CSI sequences the
// serial terminal sends:
//   F12  (07)      -> key_toggle
//   E0 75 (up)     -> key_up
//   E0 72 (down)   -> key_down
//   5A   (Enter)   -> key_enter
//   76   (Esc)     -> key_esc
// ============================================================================
module ps2_keys (
    input        clk,
    input        rst_n,
    input  [7:0] scancode,
    input        sc_valid,
    input        sc_break,
    input        sc_ext,
    output reg   key_toggle,
    output reg   key_up,
    output reg   key_down,
    output reg   key_left,
    output reg   key_right,
    output reg   key_enter,
    output reg   key_esc
);
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            key_toggle <= 1'b0; key_up <= 1'b0; key_down <= 1'b0;
            key_left <= 1'b0; key_right <= 1'b0;
            key_enter <= 1'b0;  key_esc <= 1'b0;
        end else begin
            key_toggle <= 1'b0; key_up <= 1'b0; key_down <= 1'b0;
            key_left <= 1'b0; key_right <= 1'b0;
            key_enter <= 1'b0;  key_esc <= 1'b0;

            if (sc_valid && !sc_break) begin
                if (sc_ext) begin
                    if (scancode == 8'h75)      key_up    <= 1'b1;
                    else if (scancode == 8'h72) key_down  <= 1'b1;
                    else if (scancode == 8'h6B) key_left  <= 1'b1;
                    else if (scancode == 8'h74) key_right <= 1'b1;
                end else begin
                    if (scancode == 8'h07)      key_toggle <= 1'b1;
                    else if (scancode == 8'h5A) key_enter  <= 1'b1;
                    else if (scancode == 8'h76) key_esc    <= 1'b1;
                end
            end
        end
    end
endmodule
