// ============================================================================
// usb_key_cdc - one-event CDC for a USB HID key press (ULPI clk -> 27 MHz)
//
// The ULPI link runs on ~ulpi_clk (~60 MHz); the console runs on 27 MHz.  USB
// key events are sparse, so a toggle handshake over a register-stable payload
// is used instead of a FIFO: the payload is captured in the source domain and
// only sampled after the toggle has been synchronised (>= 2 destination
// cycles later), so no multi-bit data is ever crossing on an unstable edge.
// ============================================================================
module usb_key_cdc (
    input            clk_in,
    input            rst_in_n,
    input      [7:0] key_in,
    input      [7:0] mod_in,
    input            valid_in,
    input            clk_out,
    input            rst_out_n,
    output reg [7:0] key_out,
    output reg [7:0] mod_out,
    output reg       valid_out
);
    reg [7:0] key_h, mod_h;
    reg       tog_in;
    reg [2:0] tog_sync;

    always @(posedge clk_in or negedge rst_in_n) begin
        if (!rst_in_n) begin
            key_h <= 8'd0; mod_h <= 8'd0; tog_in <= 1'b0;
        end else if (valid_in) begin
            key_h  <= key_in;
            mod_h  <= mod_in;
            tog_in <= ~tog_in;
        end
    end

    always @(posedge clk_out or negedge rst_out_n) begin
        if (!rst_out_n) begin
            tog_sync <= 3'd0; valid_out <= 1'b0; key_out <= 8'd0; mod_out <= 8'd0;
        end else begin
            tog_sync <= {tog_sync[1:0], tog_in};
            if (tog_sync[2] ^ tog_sync[1]) begin
                key_out   <= key_h;
                mod_out   <= mod_h;
                valid_out <= 1'b1;
            end else
                valid_out <= 1'b0;
        end
    end
endmodule
