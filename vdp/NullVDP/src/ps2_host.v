// ============================================================================
// ps2_host - PS/2 device (keyboard) receiver
//
// Receive-only PS/2 host: samples the device-driven 11-bit frames on the
// falling edge of ps2_clk (start, 8 data LSB-first, odd parity, stop) and
// emits a scan code per frame.  The F0 (break) and E0 (extended) prefixes are
// latched and reported alongside the code so the decoder can tell a make from
// a break and a normal from an extended key.
//
// ps2_clk / ps2_data must have pull-ups (the keyboard drives them open-drain,
// the FPGA only listens).  3-stage input filter on the clock kills the slow
// edges and glitches seen on long PMOD leads.
// ============================================================================
module ps2_host (
    input        clk,
    input        rst_n,
    input        ps2_clk,
    input        ps2_data,
    output reg [7:0] scancode,   // last scan code (prefixes stripped)
    output reg       sc_valid,   // one-cycle strobe
    output reg       sc_break,   // code was preceded by F0 (key release)
    output reg       sc_ext,     // code was preceded by E0 (extended key)
    output reg       frame_err   // toggles on a malformed frame (debug)
);
    reg clk_s1, clk_s2, clk_s3;
    reg dat_s1, dat_s2;
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            {clk_s3, clk_s2, clk_s1} <= 3'b111;
            {dat_s2, dat_s1} <= 2'b11;
        end else begin
            {clk_s3, clk_s2, clk_s1} <= {clk_s2, clk_s1, ps2_clk};
            {dat_s2, dat_s1} <= {dat_s1, ps2_data};
        end
    end

    wire clk_fall = clk_s3 & ~clk_s2;   // filtered falling edge
    wire dat_bit  = dat_s2;

    reg [3:0] bitc;
    reg [7:0] dat;
    reg       par;

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            bitc <= 4'd0; dat <= 8'd0; par <= 1'b0;
            scancode <= 8'd0; sc_valid <= 1'b0;
            sc_break <= 1'b0; sc_ext <= 1'b0; frame_err <= 1'b0;
        end else begin
            sc_valid <= 1'b0;
            if (sc_valid) begin                 // prefix flags hold through the strobe
                sc_break <= 1'b0;
                sc_ext   <= 1'b0;
            end
            if (clk_fall) begin
                if (bitc == 4'd0) begin
                    if (!dat_bit) bitc <= 4'd1;          // start bit
                end else begin
                    case (bitc)
                    4'd1, 4'd2, 4'd3, 4'd4, 4'd5, 4'd6, 4'd7, 4'd8:
                        dat[bitc - 4'd1] <= dat_bit;
                    4'd9:
                        par <= dat_bit;
                    default: begin                        // bitc == 10: stop
                        bitc <= 4'd0;
                        if (dat_bit && ((^dat) ^ par) == 1'b1) begin
                            if (dat == 8'hF0)      sc_break <= 1'b1;
                            else if (dat == 8'hE0) sc_ext   <= 1'b1;
                            else if (dat == 8'hFA || dat == 8'hAA || dat == 8'hFE) begin
                                sc_break <= 1'b0; sc_ext <= 1'b0;   // ACK/BAT/resend
                            end else begin
                                scancode <= dat;
                                sc_valid <= 1'b1;
                            end
                        end else begin
                            frame_err <= ~frame_err;
                        end
                    end
                    endcase
                    if (bitc != 4'd10) bitc <= bitc + 4'd1;
                end
            end
        end
    end
endmodule
