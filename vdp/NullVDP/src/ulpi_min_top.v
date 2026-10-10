// Minimal UART sanity top: prints "AB" ~every 0.05 s. No ULPI.
module ulpi_min_top (
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
    wire clk = clk_27m;
    reg [15:0] por = 16'd0;
    always @(posedge clk) if (por != 16'hFFFF) por <= por + 16'd1;

    assign ulpi_stp = 1'b0;
    assign ulpi_rst = 1'b1;
    assign led = por[15:12];

    reg [19:0] cnt = 20'd0;
    reg        go = 1'b0;
    always @(posedge clk) begin
        go <= 1'b0;
        if (cnt == 20'hFFFFF) begin cnt <= 20'd0; go <= 1'b1; end
        else cnt <= cnt + 20'd1;
    end

    reg [1:0] step = 2'd0;
    reg [7:0] tx_data;
    reg       tx_valid;
    wire      tx_ready, tx_busy;
    always @(*) tx_data = (step == 2'd0) ? 8'h41 : 8'h42;   // 'A','B'
    always @(posedge clk) begin
        tx_valid <= 1'b0;
        if (tx_valid) begin
            if (tx_ready) tx_valid <= 1'b0;
        end else if (go) begin
            tx_valid <= 1'b1;
            step <= (step == 2'd1) ? 2'd0 : step + 2'd1;
        end
    end

    uart_tx #(.CLK_FRE(27), .BAUD_RATE(115200)) u_tx (
        .clk(clk), .rst_n(rst_n), .tx_pin(uart_tx),
        .tx_data(tx_data), .tx_data_valid(tx_valid),
        .tx_data_ready(tx_ready), .tx_busy(tx_busy)
    );
endmodule
