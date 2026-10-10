// ============================================================================
// ULPI bring-up test using the Sipeed/Luna-generated register_window module.
// Reads USB3317 reg 0 periodically and prints "<nxt> <dir> <id0>" on UART0.
// ============================================================================
module ulpi_regwin_top (
    input        clk_27m,
    input        rst_n,
    input        ulpi_clk,
    input        ulpi_dir,
    input        ulpi_nxt,
    inout  [7:0] ulpi_data,
    output       ulpi_stp,
    output       ulpi_rst,
    output       uart_tx,
    output [3:0] led
);

    wire int_clk = ~ulpi_clk;             // board ULPI clock is inverted
    reg  [7:0] por = 8'd0;
    always @(posedge int_clk) if (por != 8'hFF) por <= por + 8'd1;
    wire usb_rst = ~(rst_n & (por == 8'hFF));   // active-high reset for register_window

    assign ulpi_rst = 1'b1;               // USB3317 RESETB released
    assign led = {2'b00, ulpi_dir, ulpi_nxt};

    reg  [5:0] address  = 6'd0;
    reg        read_req = 1'b0;
    wire [7:0] ulpi_data_out, read_data;
    wire       ulpi_data_oe, ulpi_stop, rw_busy, rw_done;

    register_window rw (
        .usb_clk      (int_clk),
        .usb_rst      (usb_rst),
        .busy         (rw_busy),
        .ulpi_data_in (ulpi_data),
        .ulpi_dir     (ulpi_dir),
        .ulpi_next    (ulpi_nxt),
        .ulpi_data_out(ulpi_data_out),
        .ulpi_stop    (ulpi_stop),
        .done         (rw_done),
        .address      (address),
        .write_data   (8'h00),
        .read_request (read_req),
        .write_request(1'b0),
        .read_data    (read_data)
    );

    assign ulpi_data = ulpi_data_oe ? ulpi_data_out : 8'hz;
    assign ulpi_stp  = ulpi_stop;

    reg [23:0] tick_cnt = 24'd0;
    reg        tick = 1'b0;
    always @(posedge int_clk) begin
        tick <= 1'b0;
        if (tick_cnt == 24'hFFFFFF) begin tick_cnt <= 24'd0; tick <= 1'b1; end
        else tick_cnt <= tick_cnt + 24'd1;
    end

    reg [7:0] id0 = 8'h00;
    reg [3:0] st = 4'd0;
    reg [19:0] to = 20'd0;
    reg [3:0] nxt_cnt = 4'd0, dir_cnt = 4'd0;

    function [7:0] nib; input [3:0] n; begin nib = (n<10)?(8'h30+n):(8'h37+n); end endfunction

    reg [3:0] byte_idx = 4'd0;
    reg [7:0] out_byte;
    always @(*) begin
        case (byte_idx)
        4'd0: out_byte = nib(nxt_cnt);
        4'd1: out_byte = 8'h20;
        4'd2: out_byte = nib(dir_cnt);
        4'd3: out_byte = 8'h20;
        4'd4: out_byte = nib(id0[7:4]);
        4'd5: out_byte = nib(id0[3:0]);
        4'd6: out_byte = 8'h0D;
        4'd7: out_byte = 8'h0A;
        default: out_byte = 8'h00;
        endcase
    end

    reg  [7:0] tx_data = 8'd0;
    reg        tx_valid = 1'b0;
    wire       tx_ready, tx_busy;
    uart_tx #(.CLK_FRE(60), .BAUD_RATE(115200)) u_tx (
        .clk(int_clk), .rst_n(rst_n), .tx_pin(uart_tx),
        .tx_data(tx_data), .tx_data_valid(tx_valid),
        .tx_data_ready(tx_ready), .tx_busy(tx_busy)
    );

    always @(posedge int_clk or negedge rst_n) begin
        if (!rst_n) begin
            st <= 4'd0; read_req <= 1'b0; address <= 6'd0; to <= 20'd0;
            tx_valid <= 1'b0; tx_data <= 8'd0; byte_idx <= 4'd0;
            nxt_cnt <= 4'd0; dir_cnt <= 4'd0;
        end else begin
            if (st == 4'd1) begin
                to <= to + 20'd1;
                if (ulpi_nxt && nxt_cnt != 4'hF) nxt_cnt <= nxt_cnt + 4'd1;
                if (ulpi_dir && dir_cnt != 4'hF) dir_cnt <= dir_cnt + 4'd1;
            end

            case (st)
            4'd0: if (tick && !rw_busy) begin
                      nxt_cnt <= 4'd0; dir_cnt <= 4'd0; to <= 20'd0;
                      address <= 6'd0; read_req <= 1'b1; st <= 4'd1;
                  end
            4'd1: begin
                      read_req <= 1'b0;
                      if (rw_done) begin id0 <= read_data; st <= 4'd2; end
                      else if (to == 20'hFFFFF) st <= 4'd2;
                  end
            4'd2: begin
                      if (tx_valid) begin
                          if (tx_ready) tx_valid <= 1'b0;
                      end else if (byte_idx <= 4'd7) begin
                          tx_data  <= out_byte;
                          tx_valid <= 1'b1;
                          byte_idx <= byte_idx + 4'd1;
                      end else begin
                          byte_idx <= 4'd0;
                          st <= 4'd0;
                      end
                  end
            default: st <= 4'd0;
            endcase
        end
    end

endmodule
