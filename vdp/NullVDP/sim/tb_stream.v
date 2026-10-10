// ============================================================================
// tb_stream - replay a captured console byte stream through the RTL
//
// Reads space-separated hex bytes (e.g. /tmp/opencode/stream.hex), drives
// terminal_handler -> gfx_engine -> gfx_buffer, and watchdog-reports if the
// handler stops accepting bytes (a graphics command that never finishes).
// ============================================================================
`timescale 1ns/1ps

module tb_stream;

    reg clk = 0, rst_n = 0;
    always #5 clk = ~clk;

    reg [7:0] mem [0:65535];
    integer n = 0, idx = 0, wd = 0;
    integer dx, dy, daddr, didx, dc;
    integer df;
    reg [7:0] pr, pg, pb;

    reg  [7:0] rx_data = 0;
    reg        rx_valid = 0;
    wire       rx_ready;

    wire        char_we, attr_we;
    wire [11:0] char_waddr, attr_waddr;
    wire [7:0]  char_wdata;
    wire [15:0] attr_wdata;
    wire [6:0]  cursor_x;
    wire [5:0]  cursor_y;
    wire        cursor_visible, scroll_off;
    wire        key_toggle, key_up, key_down, key_enter, key_esc;
    wire        gfx_active;
    wire [2:0]  gfx_fg, gfx_bg;
    wire        gfx_start;
    wire [1:0]  gfx_shape;
    wire [2:0]  gfx_colour;
    wire [10:0] gfx_ax, gfx_bx;
    wire [8:0]  gfx_ay, gfx_by;
    wire        gfx_busy;
    wire [2:0]  gfx_we, gfx_wdata;
    wire [17:0] gfx_waddr;
    wire [2:0]  gfx_rd_data;

    terminal_handler th (
        .clk(clk), .rst_n(rst_n),
        .rx_data(rx_data), .rx_valid(rx_valid), .rx_ready(rx_ready),
        .char_we(char_we), .char_waddr(char_waddr), .char_wdata(char_wdata),
        .attr_we(attr_we), .attr_waddr(attr_waddr), .attr_wdata(attr_wdata),
        .cursor_x(cursor_x), .cursor_y(cursor_y),
        .cursor_visible(cursor_visible), .scroll_off(scroll_off),
        .key_toggle(key_toggle), .key_up(key_up), .key_down(key_down),
        .key_enter(key_enter), .key_esc(key_esc),
        .gfx_active(gfx_active), .gfx_fg(gfx_fg), .gfx_bg(gfx_bg),
        .gfx_start(gfx_start), .gfx_shape(gfx_shape), .gfx_colour(gfx_colour),
        .gfx_ax(gfx_ax), .gfx_ay(gfx_ay), .gfx_bx(gfx_bx), .gfx_by(gfx_by),
        .gfx_busy(gfx_busy)
    );

    gfx_engine eng (
        .clk(clk), .rst_n(rst_n),
        .start(gfx_start), .shape(gfx_shape), .colour(gfx_colour),
        .ax(gfx_ax), .ay(gfx_ay), .bx(gfx_bx), .by(gfx_by),
        .busy(gfx_busy),
        .we(gfx_we), .waddr(gfx_waddr), .wdata(gfx_wdata)
    );

    gfx_buffer gb (
        .clk_rd(clk), .rd_addr(18'd0), .rd_data(gfx_rd_data),
        .clk_wr(clk), .we(gfx_we), .wr_addr(gfx_waddr), .wr_data(gfx_wdata)
    );

    char_buffer cb (
        .clk_rd(clk), .rd_addr(12'd0), .rd_data(),
        .clk_wr(clk), .wr_en(char_we), .wr_addr(char_waddr), .wr_data(char_wdata)
    );

    // trace the parser over the first bytes
    always @(posedge clk) begin
        if (rx_valid && idx < 26)
            $display("idx=%0d state=%0d rx=%02x", idx, th.state, rx_data);
    end

    initial begin
        $readmemh("/tmp/opencode/stream.hex", mem);
        n = 0;
        while (n < 65536 && mem[n] !== 8'hxx) n = n + 1;
        $display("stream length = %0d bytes", n);

        repeat (4) @(posedge clk);
        rst_n = 1;
        @(posedge clk);

        for (idx = 0; idx < n; idx = idx + 1) begin
            // hold the byte (like the SPI slave holds tx_valid until tx_ready)
            rx_data  <= mem[idx];
            rx_valid <= 1'b1;
            wd = 0;
            @(posedge clk);
            while (!rx_ready) begin
                @(posedge clk);
                wd = wd + 1;
                if (wd > 400000) begin
                    $display("HANG at idx=%0d byte=%02x  th.state=%0d gfx_cmd=%02x gfx_left=%0d",
                             idx, mem[idx], th.state, th.gfx_cmd_r, th.gfx_left);
                    $display("  engine state=%0d busy=%b kind=%0d cx=%0d cy=%0d",
                             eng.state, gfx_busy, eng.kind, eng.cx, eng.cy);
                    $finish;
                end
            end
            rx_valid <= 1'b0;
        end

        // drain any in-flight command
        wd = 0;
        while (gfx_busy || !rx_ready) begin
            @(posedge clk);
            wd = wd + 1;
            if (wd > 400000) begin
                $display("HANG draining: engine state=%0d kind=%0d cx=%0d cy=%0d",
                         eng.state, eng.kind, eng.cx, eng.cy);
                $finish;
            end
        end
        $display("PASS: replay completed, no hang");

        // ---- dump the bitmap layer + text grid for inspection -------------
        df = $fopen("/tmp/opencode/gfx.ppm", "wb");
        $fwrite(df, "P6\n640 256\n255\n");
        for (dy = 0; dy < 256; dy = dy + 1) begin
            for (dx = 0; dx < 640; dx = dx + 1) begin
                daddr = dy * 640 + dx;
                didx = {gb.p2[daddr], gb.p1[daddr], gb.p0[daddr]};
                case (didx)
                    3'd0: begin pr=8'h00; pg=8'h00; pb=8'h00; end
                    3'd1: begin pr=8'h55; pg=8'h55; pb=8'h55; end
                    3'd2: begin pr=8'hAA; pg=8'hAA; pb=8'hAA; end
                    3'd3: begin pr=8'hFF; pg=8'hFF; pb=8'hFF; end
                    3'd4: begin pr=8'h00; pg=8'h00; pb=8'hAA; end
                    3'd5: begin pr=8'h00; pg=8'hAA; pb=8'h00; end
                    3'd6: begin pr=8'hAA; pg=8'h00; pb=8'h00; end
                    3'd7: begin pr=8'hFF; pg=8'hFF; pb=8'h55; end
                endcase
                $fwrite(df, "%c%c%c", pr, pg, pb);
            end
        end
        $fclose(df);

        df = $fopen("/tmp/opencode/text.txt", "w");
        for (dy = 0; dy < 30; dy = dy + 1) begin
            for (dx = 0; dx < 80; dx = dx + 1) begin
                dc = cb.mem[dy*80+dx];
                if (dc < 8'h20 || dc > 8'h7e) dc = 8'h2e;
                $fwrite(df, "%c", dc);
            end
            $fwrite(df, "\n");
        end
        $fclose(df);
        $display("wrote /tmp/opencode/gfx.ppm and text.txt");
        $finish;
    end

    initial begin
        #200000000;
        $display("TIMEOUT (global)");
        $finish;
    end

endmodule
