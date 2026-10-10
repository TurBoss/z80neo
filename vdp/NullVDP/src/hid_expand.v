// ============================================================================
// hid_expand - USB HID boot key -> console byte stream
//
// Maps a HID usage ID (plus the shift modifier) to the byte(s) the z80neo
// console protocol expects: printable ASCII / CR / BS / TAB / ESC, with the
// arrow keys and F12 expanded to the same CSI sequences the serial terminal
// already sends (ESC [ A/B/C/D and ESC [ 2 4 ~), so the local menu reacts to
// the USB keyboard with no extra wiring.
//
// valid/ready handshake: `chr` is held with `chr_valid` high until `ready`.
// ============================================================================
module hid_expand (
    input        clk,
    input        rst_n,
    input  [7:0] key,        // HID usage ID (0x00 = none)
    input  [7:0] mod,        // HID boot report modifier byte
    input        key_valid,  // 1-cycle pulse: new key press
    output reg [7:0] chr,
    output reg       chr_valid,
    input            ready
);
    wire shift = mod[1] | mod[5];   // left / right shift

    reg [7:0] kbuf [0:4];
    reg [2:0] nlen;
    reg [2:0] seq_idx;
    reg       active;

    task load_key;
        input [7:0] k;
        input       sh;
        begin
            nlen = 3'd1;
            kbuf[0] = k;                     // default: literal (handled below)
            case (k)
            8'h04: kbuf[0] = sh ? 8'h41 : 8'h61; // a A
            8'h05: kbuf[0] = sh ? 8'h42 : 8'h62;
            8'h06: kbuf[0] = sh ? 8'h43 : 8'h63;
            8'h07: kbuf[0] = sh ? 8'h44 : 8'h64;
            8'h08: kbuf[0] = sh ? 8'h45 : 8'h65;
            8'h09: kbuf[0] = sh ? 8'h46 : 8'h66;
            8'h0A: kbuf[0] = sh ? 8'h47 : 8'h67;
            8'h0B: kbuf[0] = sh ? 8'h48 : 8'h68;
            8'h0C: kbuf[0] = sh ? 8'h49 : 8'h69;
            8'h0D: kbuf[0] = sh ? 8'h4A : 8'h6A;
            8'h0E: kbuf[0] = sh ? 8'h4B : 8'h6B;
            8'h0F: kbuf[0] = sh ? 8'h4C : 8'h6C;
            8'h10: kbuf[0] = sh ? 8'h4D : 8'h6D;
            8'h11: kbuf[0] = sh ? 8'h4E : 8'h6E;
            8'h12: kbuf[0] = sh ? 8'h4F : 8'h6F;
            8'h13: kbuf[0] = sh ? 8'h50 : 8'h70;
            8'h14: kbuf[0] = sh ? 8'h51 : 8'h71;
            8'h15: kbuf[0] = sh ? 8'h52 : 8'h72;
            8'h16: kbuf[0] = sh ? 8'h53 : 8'h73;
            8'h17: kbuf[0] = sh ? 8'h54 : 8'h74;
            8'h18: kbuf[0] = sh ? 8'h55 : 8'h75;
            8'h19: kbuf[0] = sh ? 8'h56 : 8'h76;
            8'h1A: kbuf[0] = sh ? 8'h57 : 8'h77;
            8'h1B: kbuf[0] = sh ? 8'h58 : 8'h78;
            8'h1C: kbuf[0] = sh ? 8'h59 : 8'h79;
            8'h1D: kbuf[0] = sh ? 8'h5A : 8'h7A;
            8'h1E: kbuf[0] = sh ? 8'h21 : 8'h31;
            8'h1F: kbuf[0] = sh ? 8'h40 : 8'h32;
            8'h20: kbuf[0] = sh ? 8'h23 : 8'h33;
            8'h21: kbuf[0] = sh ? 8'h24 : 8'h34;
            8'h22: kbuf[0] = sh ? 8'h25 : 8'h35;
            8'h23: kbuf[0] = sh ? 8'h5E : 8'h36;
            8'h24: kbuf[0] = sh ? 8'h26 : 8'h37;
            8'h25: kbuf[0] = sh ? 8'h2A : 8'h38;
            8'h26: kbuf[0] = sh ? 8'h28 : 8'h39;
            8'h27: kbuf[0] = sh ? 8'h29 : 8'h30;
            8'h28: kbuf[0] = 8'h0D;          // Enter
            8'h29: kbuf[0] = 8'h1B;          // Esc
            8'h2A: kbuf[0] = 8'h08;          // Backspace
            8'h2B: kbuf[0] = 8'h09;          // Tab
            8'h2C: kbuf[0] = 8'h20;          // Space
            8'h2D: kbuf[0] = sh ? 8'h5F : 8'h2D;
            8'h2E: kbuf[0] = sh ? 8'h2B : 8'h3D;
            8'h2F: kbuf[0] = sh ? 8'h7B : 8'h5B;
            8'h30: kbuf[0] = sh ? 8'h7D : 8'h5D;
            8'h31: kbuf[0] = sh ? 8'h7C : 8'h5C;
            8'h33: kbuf[0] = sh ? 8'h3A : 8'h3B;
            8'h34: kbuf[0] = sh ? 8'h22 : 8'h27;
            8'h35: kbuf[0] = sh ? 8'h7E : 8'h60;
            8'h36: kbuf[0] = sh ? 8'h3C : 8'h2C;
            8'h37: kbuf[0] = sh ? 8'h3E : 8'h2E;
            8'h38: kbuf[0] = sh ? 8'h3F : 8'h2F;
            8'h4F: begin kbuf[0]=8'h1B; kbuf[1]=8'h5B; kbuf[2]=8'h43; nlen=3'd3; end // right
            8'h50: begin kbuf[0]=8'h1B; kbuf[1]=8'h5B; kbuf[2]=8'h44; nlen=3'd3; end // left
            8'h51: begin kbuf[0]=8'h1B; kbuf[1]=8'h5B; kbuf[2]=8'h42; nlen=3'd3; end // down
            8'h52: begin kbuf[0]=8'h1B; kbuf[1]=8'h5B; kbuf[2]=8'h41; nlen=3'd3; end // up
            8'h45: begin kbuf[0]=8'h1B; kbuf[1]=8'h5B; kbuf[2]=8'h32;                // F12
                         kbuf[3]=8'h34; kbuf[4]=8'h7E; nlen=3'd5; end
            default: nlen = 3'd0;
            endcase
        end
    endtask

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            chr <= 8'd0; chr_valid <= 1'b0; active <= 1'b0; seq_idx <= 3'd0;
            nlen <= 3'd0;
        end else begin
            if (key_valid && !active)
                load_key(key, shift);       // blocking: fills kbuf[] / nlen

            if (!active) begin
                if (nlen != 3'd0) begin
                    chr       <= kbuf[0];
                    chr_valid <= 1'b1;
                    seq_idx    <= 3'd1;
                    active     <= 1'b1;
                end
            end else if (ready) begin
                if (seq_idx == nlen) begin
                    chr_valid <= 1'b0;
                    active    <= 1'b0;
                    nlen      <= 3'd0;      // consumed: don't re-send
                end else begin
                    chr    <= kbuf[seq_idx];
                    seq_idx <= seq_idx + 3'd1;
                end
            end
        end
    end
endmodule
