// ============================================================================
// ULPI register check (one op per 0.3 s tick):
//   0: write 0x04 = 0x4C      1: read 0x04 -> rf0  (expect 0x4C)
//   2: write 0x0A = 0x5A      3: read 0x0A -> rf1  (expect 0x5A)
//   4: read  0x00       -> rf2 (expect 0x24)
//   5: write 0x06 = 0x26      6: read 0x06 -> rf3  (expect 0x26)
//   7: print "<rf0> <rf1> <rf2> <rf3>"
// ============================================================================
module ulpi_test_top (
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

    wire ulclk = ~ulpi_clk;

    reg [7:0] por = 8'd0;
    always @(posedge ulclk) if (por != 8'hFF) por <= por + 8'd1;
    wire ul_rst_n = rst_n & (por == 8'hFF);

    assign ulpi_rst = 1'b1;
    assign led = {2'b00, ulpi_dir, ulpi_nxt};

    reg        start = 1'b0;
    reg        rw    = 1'b0;
    reg  [5:0] addr  = 6'd0;
    reg  [7:0] wdata = 8'd0;
    wire [7:0] rdata;
    wire       done, phy_busy;

    wire [7:0] phy_out;
    wire       phy_oe;
    assign ulpi_data = phy_oe ? phy_out : 8'bz;

    ulpi_phy u_phy (
        .clk(ulclk), .rst_n(ul_rst_n),
        .start(start), .reg_write(rw), .addr(addr), .wdata(wdata),
        .rdata(rdata), .done(done), .busy(phy_busy), .stp(),
        .ulpi_data_in(ulpi_data), .ulpi_data_out(phy_out), .ulpi_data_oe(phy_oe),
        .ulpi_dir(ulpi_dir), .ulpi_nxt(ulpi_nxt)
    );

    reg [23:0] tick_cnt = 24'd0;
    reg        tick = 1'b0;
    always @(posedge ulclk) begin
        tick <= 1'b0;
        if (tick_cnt == 24'hFFFFFF) begin tick_cnt <= 24'd0; tick <= 1'b1; end
        else tick_cnt <= tick_cnt + 24'd1;
    end

    reg [7:0] rf [0:3];
    reg [3:0] step = 4'd0;
    reg [3:0] st = 4'd0;
    reg [19:0] to = 20'd0;

    function [7:0] nib; input [3:0] n; begin nib = (n<10)?(8'h30+n):(8'h37+n); end endfunction

    reg [5:0] byte_idx = 6'd0;
    wire [3:0] pi = byte_idx[4:2];
    reg [7:0] out_byte;
    always @(*) begin
        case (byte_idx[1:0])
        2'd0: out_byte = nib(rf[pi][7:4]);
        2'd1: out_byte = nib(rf[pi][3:0]);
        2'd2: out_byte = 8'h20;
        2'd3: out_byte = (pi == 4'd3) ? 8'h0D : 8'h20;
        endcase
    end

    reg  [7:0] tx_data = 8'd0;
    reg        tx_valid = 1'b0;
    wire       tx_ready, tx_busy_u;
    uart_tx #(.CLK_FRE(60), .BAUD_RATE(115200)) u_tx (
        .clk(ulclk), .rst_n(ul_rst_n), .tx_pin(uart_tx),
        .tx_data(tx_data), .tx_data_valid(tx_valid),
        .tx_data_ready(tx_ready), .tx_busy(tx_busy_u)
    );

    always @(posedge ulclk or negedge ul_rst_n) begin
        if (!ul_rst_n) begin
            st <= 4'd0; start <= 1'b0; addr <= 6'd0; to <= 20'd0; rw <= 1'b0;
            wdata <= 8'd0; tx_valid <= 1'b0; tx_data <= 8'd0; byte_idx <= 6'd0; step <= 4'd0;
        end else begin
            start <= 1'b0;
            if (st == 4'd1) to <= to + 20'd1;

            case (st)
            4'd0: if (tick) begin
                      to <= 20'd0;
                      if (step == 4'd6) st <= 4'd2;
                      else begin
                          case (step)
                          4'd0: begin addr <= 6'h00; rw <= 1'b0; start <= 1'b1; end
                          4'd1: begin addr <= 6'h04; rw <= 1'b1; wdata <= 8'h4C; start <= 1'b1; end
                          4'd2: begin addr <= 6'h04; rw <= 1'b0; start <= 1'b1; end
                          4'd3: begin addr <= 6'h0A; rw <= 1'b1; wdata <= 8'h5A; start <= 1'b1; end
                          4'd4: begin addr <= 6'h0A; rw <= 1'b0; start <= 1'b1; end
                          4'd5: begin addr <= 6'h00; rw <= 1'b0; start <= 1'b1; end
                          default: ;
                          endcase
                          st <= 4'd1;
                      end
                  end
            4'd1: if (done) begin
                      case (step)
                      4'd0: rf[0] <= rdata;
                      4'd2: rf[1] <= rdata;
                      4'd4: rf[2] <= rdata;
                      4'd5: rf[3] <= rdata;
                      default: ;
                      endcase
                      step <= step + 4'd1;
                      st   <= 4'd0;
                  end else if (to == 20'hFFFFF) begin
                      step <= step + 4'd1;
                      st   <= 4'd0;
                  end
            4'd2: begin
                      if (tx_valid) begin
                          if (tx_ready) tx_valid <= 1'b0;
                      end else if (byte_idx <= 6'd15) begin
                          tx_data  <= out_byte;
                          tx_valid <= 1'b1;
                          byte_idx <= byte_idx + 6'd1;
                      end else begin
                          byte_idx <= 6'd0;
                          step     <= 4'd0;
                          st       <= 4'd0;
                      end
                  end
            default: st <= 4'd0;
            endcase
        end
    end

endmodule
