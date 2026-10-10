// ============================================================================
// tb_spi_gfx - replay a captured stream through the real SPI slave + handler
//
// A master mimics the firmware (status-read frame, then a TX frame when
// TX_READY), driving spi_slave -> terminal_handler -> gfx_engine -> buffers.
// Dumps the text grid so a desync shows up as stray glyphs.
// ============================================================================
`timescale 1ns/1ps

module tb_spi_gfx;

    reg clk = 0, rst_n = 0;
    always #5 clk = ~clk;

    reg  spi_sck = 0, spi_mosi = 0, spi_cs_n = 1;
    wire spi_miso;

    wire       tx_byte_we;
    wire [7:0] s_tx_byte;
    wire       s_tx_valid;
    wire       th_rx_ready;
    wire       spi_active;
    wire [7:0] s_rx_byte = 8'h00;
    reg        s_rx_valid = 0;

    spi_slave slave (
        .clk(clk), .rst_n(rst_n),
        .spi_sck(spi_sck), .spi_mosi(spi_mosi), .spi_cs_n(spi_cs_n),
        .spi_miso(spi_miso),
        .tx_byte(s_tx_byte), .tx_valid(s_tx_valid), .tx_ready(th_rx_ready),
        .spi_active(spi_active),
        .rx_byte(s_rx_byte), .rx_valid(s_rx_valid)
    );

    wire        char_we, attr_we;
    wire [11:0] char_waddr, attr_waddr;
    wire [7:0]  char_wdata;
    wire [15:0] attr_wdata;
    wire [6:0]  cursor_x;
    wire [5:0]  cursor_y;
    wire        cursor_visible;
    wire [5:0]  scroll_off;
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
    integer gfx_writes = 0;

    terminal_handler th (
        .clk(clk), .rst_n(rst_n),
        .rx_data(s_tx_byte), .rx_valid(s_tx_valid), .rx_ready(th_rx_ready),
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
        .busy(gfx_busy), .we(gfx_we), .waddr(gfx_waddr), .wdata(gfx_wdata)
    );

    gfx_buffer gb (
        .clk_rd(clk), .rd_addr(18'd0), .rd_data(gfx_rd_data),
        .clk_wr(clk), .we(gfx_we), .wr_addr(gfx_waddr), .wr_data(gfx_wdata)
    );

    char_buffer cb (
        .clk_rd(clk), .rd_addr(12'd0), .rd_data(),
        .clk_wr(clk), .wr_en(char_we), .wr_addr(char_waddr), .wr_data(char_wdata)
    );

    reg [7:0] mem [0:65535];
    integer n = 0, idx = 0;

    // ---- firmware-like SPI master ------------------------------------------
    reg [7:0] f_status, f_rdata, f_ack;
    reg [31:0] f_response;
    function [7:0] crc8_24;
        input [23:0] value;
        integer byte, bit;
        reg [7:0] crc, data;
        begin
            crc = 0;
            for (byte = 2; byte >= 0; byte = byte - 1) begin
                data = value >> (byte * 8);
                crc = crc ^ data;
                for (bit = 0; bit < 8; bit = bit + 1)
                    crc = crc[7] ? ((crc << 1) ^ 8'h07) : (crc << 1);
            end
            crc8_24 = crc;
        end
    endfunction

    function [31:0] frame;
        input [7:0] kind, sequence, data;
        reg [23:0] payload;
        begin
            payload = {kind, sequence, data};
            frame = {payload, crc8_24(payload)};
        end
    endfunction

    task spi_frame;
        input [31:0] out_frame;
        integer i;
        begin
            f_status = 8'h00; f_rdata = 8'h00; f_ack = 8'h00;
            f_response = 32'd0;
            spi_cs_n = 0; #40;
            for (i = 31; i >= 0; i = i - 1) begin
                spi_mosi = out_frame[i];
                #40; spi_sck = 1; #40;
                f_response = {f_response[30:0], spi_miso};
                spi_sck = 0; #40;
            end
            spi_cs_n = 1; #40;
            f_status = f_response[31:24];
            f_ack = f_response[23:16];
            f_rdata = f_response[15:8];
        end
    endtask

    integer df, dy, dx, dc;
    reg [7:0] seq_val;
    initial begin
        $readmemh("/tmp/opencode/stream.hex", mem);
        n = 0;
        while (n < 65536 && mem[n] !== 8'hxx) n = n + 1;
        $display("stream length = %0d", n);

        repeat (4) @(posedge clk); rst_n = 1; repeat (4) @(posedge clk);

        seq_val = 0;
        for (idx = 0; idx < n; idx = idx + 1) begin
            spi_frame(frame(8'h5A, 0, 0));            // poll
            while (!(f_status & 8'h01))              // TX_READY
                spi_frame(frame(8'h5A, 0, 0));
            spi_frame(frame(8'hA5, seq_val, mem[idx]));
            spi_frame(frame(8'h5A, 0, 0));
            while (f_ack != seq_val)
                spi_frame(frame(8'h5A, 0, 0));
            seq_val = seq_val + 1;
        end
        $display("stream sent through SPI slave");

        repeat (200) @(posedge clk);

        $display("graphics active=%b writes=%0d", gfx_active, gfx_writes);
        if (gfx_writes == 0)
            $display("FAIL: no graphics writes reached the framebuffer");

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
        $display("wrote text.txt");
        $finish;
    end

    always @(posedge clk)
        if (|gfx_we) gfx_writes = gfx_writes + 1;

    initial begin
        #500000000;
        $display("TIMEOUT");
        $finish;
    end

endmodule
