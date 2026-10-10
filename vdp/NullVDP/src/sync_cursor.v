// ============================================================================
// Cursor/scroll synchronizer (CDC)
// 2-FF synchronizer from the 27 MHz system domain to the 25 MHz pixel domain
// ============================================================================
module sync_cursor (
    input            clk_in,      // 27 MHz source clock
    input            clk_out,     // 25 MHz destination clock
    input            rst_n_in,    // reset in source domain
    input            rst_n_out,   // reset in dest domain
    input      [6:0] cur_x_in,
    input      [5:0] cur_y_in,
    input            cur_vis_in,
    input      [5:0] off_in,
    output reg [6:0] cur_x_out,
    output reg [5:0] cur_y_out,
    output reg       cur_vis_out,
    output reg [5:0] off_out
);

    reg [6:0] x_src;
    reg [5:0] y_src;
    reg       vis_src;
    reg [5:0] off_src;

    always @(posedge clk_in or negedge rst_n_in) begin
        if (!rst_n_in) begin
            x_src   <= 7'd0;
            y_src   <= 6'd0;
            vis_src <= 1'b0;
            off_src <= 6'd0;
        end else begin
            x_src   <= cur_x_in;
            y_src   <= cur_y_in;
            vis_src <= cur_vis_in;
            off_src <= off_in;
        end
    end

    reg [6:0] x_s1, x_s2;
    reg [5:0] y_s1, y_s2;
    reg       vis_s1, vis_s2;
    reg [5:0] off_s1, off_s2;

    always @(posedge clk_out or negedge rst_n_out) begin
        if (!rst_n_out) begin
            x_s1 <= 7'd0;  x_s2 <= 7'd0;
            y_s1 <= 6'd0;  y_s2 <= 6'd0;
            vis_s1 <= 1'b0; vis_s2 <= 1'b0;
            off_s1 <= 6'd0; off_s2 <= 6'd0;
        end else begin
            x_s1   <= x_src;
            y_s1   <= y_src;
            vis_s1 <= vis_src;
            off_s1 <= off_src;
            x_s2   <= x_s1;
            y_s2   <= y_s1;
            vis_s2 <= vis_s1;
            off_s2 <= off_s1;
        end
    end

    always @(posedge clk_out or negedge rst_n_out) begin
        if (!rst_n_out) begin
            cur_x_out   <= 7'd0;
            cur_y_out   <= 6'd0;
            cur_vis_out <= 1'b0;
            off_out     <= 6'd0;
        end else begin
            cur_x_out   <= x_s2;
            cur_y_out   <= y_s2;
            cur_vis_out <= vis_s2;
            off_out     <= off_s2;
        end
    end

endmodule
