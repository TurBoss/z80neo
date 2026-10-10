// ============================================================================
// tb_ulpi_host - run ulpi_host against the PHY model and check the packets it
// emits for the first enumeration step (GET_DESCRIPTOR device, 8 bytes):
//   SETUP token  : PID D, 3 bytes, field=0, CRC5=0x02
//   DATA0 packet : PID 3, 10 bytes, 80 06 00 01 00 00 08 00 + CRC16 EB 94
// ============================================================================
`timescale 1ns/1ps
module tb_ulpi_host;
    wire       ulpi_clk;
    wire [7:0] ulpi_data;
    wire       ulpi_dir, ulpi_nxt, ulpi_stp;
    wire [3:0] tx_pid;
    wire [4:0] tx_len;
    wire [95:0] tx_data;
    wire       tx_stb;

    ulpi_phy_model phy (
        .ulpi_clk(ulpi_clk), .ulpi_data(ulpi_data),
        .ulpi_dir(ulpi_dir), .ulpi_nxt(ulpi_nxt), .ulpi_stp(ulpi_stp),
        .tx_pid(tx_pid), .tx_len(tx_len), .tx_data(tx_data), .tx_stb(tx_stb)
    );

    wire ulclk = ~ulpi_clk;
    reg  rst_n = 0;

    wire [63:0] dbg;
    wire [7:0]  dbg_tx_byte;
    wire        dbg_tx_stb;
    wire [7:0]  hid_key;
    wire        hid_key_valid;

    ulpi_host #(.RESET_CYCLES(40), .SETTLE_CYCLES(40), .DETECT_CYCLES(40), .ADDR_CYCLES(40)) host (
        .clk(ulclk), .rst_n(rst_n),
        .ulpi_data(ulpi_data), .ulpi_dir(ulpi_dir), .ulpi_nxt(ulpi_nxt),
        .ulpi_stp(ulpi_stp),
        .dbg(dbg), .dbg_tx_byte(dbg_tx_byte), .dbg_tx_stb(dbg_tx_stb),
        .hid_key(hid_key), .hid_key_valid(hid_key_valid)
    );

    integer errors = 0;
    integer npkt   = 0;
    reg     got_setup = 1'b0, got_data = 1'b0;
    reg [95:0] setup_w, data_w;
    integer    setup_len = 0;

    // collect the link's real TX bytes between PHY packet delimiters
    reg [7:0] cap [0:15];
    integer   ncap = 0;

    always @(posedge ulclk) begin
        if (dbg_tx_stb) begin
            cap[ncap] = dbg_tx_byte;
            ncap = ncap + 1;
        end
        if (tx_stb) begin
            npkt = npkt + 1;
            $write("TX#%0d pid=%X len=%0d :", npkt, tx_pid, ncap);
            for (integer j = 0; j < ncap; j = j + 1) $write(" %02X", cap[j]);
            $write("\n");
            // packet bytes: cap[0] = command (0x40|pid), cap[1..] = payload
            if (tx_pid == 4'hD && !got_setup) begin
                got_setup = 1'b1;
                setup_len = ncap;
                setup_w = {80'd0, cap[3], cap[2], cap[1]};  // cap[3] must be the next packet: len check below
            end
            if (tx_pid == 4'h3 && !got_data && ncap >= 11) begin
                got_data = 1'b1;
                data_w = {16'd0, cap[10],cap[9],cap[8],cap[7],cap[6],cap[5],cap[4],cap[3],cap[2],cap[1]};
            end
            ncap = 0;
        end
    end

    initial begin
        repeat (5) @(posedge ulclk);
        rst_n = 1;

        // wait until we have seen the SETUP token and the DATA0 packet
        while (!(got_setup && got_data)) @(posedge ulclk);
        repeat (20) @(posedge ulclk);

        if (setup_w[7:0] !== 8'h00 || setup_w[15:8] !== 8'h10 || setup_len !== 3) begin
            $display("FAIL: SETUP token bytes (want 4D 00 10)"); errors = errors + 1;
        end else $display("ok:   SETUP token 4D 00 10 (addr 0, ep 0, CRC5 0x02)");

        if (data_w[7:0]  !== 8'h80 || data_w[15:8]  !== 8'h06 ||
            data_w[23:16]!== 8'h00 || data_w[31:24]!== 8'h01 ||
            data_w[39:32]!== 8'h00 || data_w[47:40]!== 8'h00 ||
            data_w[55:48]!== 8'h08 || data_w[63:56]!== 8'h00 ||
            data_w[71:64]!== 8'hEB || data_w[79:72]!== 8'h94) begin
            $display("FAIL: DATA0 GET_DESCRIPTOR payload/CRC"); errors = errors + 1;
        end else $display("ok:   DATA0 80 06 00 01 00 00 08 00 + CRC16 EB 94");

        if (errors == 0) $display("ULPI HOST TB PASSED");
        else             $display("ULPI HOST TB FAILED (%0d)", errors);
        $finish;
    end

    initial begin
        #5000000;
        $display("ULPI HOST TB TIMEOUT (setup=%b data=%b npkt=%0d)", got_setup, got_data, npkt);
        $finish;
    end
endmodule
