// ============================================================================
// TMDS Encoder - 8b/10b encoding for DVI/HDMI
// Based on DVI 1.0 specification
// ============================================================================
//
// During active video (de=1): encodes 8-bit pixel data using transition-minimized
// encoding with DC balancing.
//
// During blanking (de=0): outputs 10-bit control codes based on ctrl[1:0]:
//   ctrl = 00: 10'b1101010100
//   ctrl = 01: 10'b0010101011
//   ctrl = 10: 10'b0101010100
//   ctrl = 11: 10'b1010101011

module tmds_encoder (
    input            clk,
    input            rst_n,
    input      [7:0] data,       // 8-bit pixel data
    input      [1:0] ctrl,       // {VSYNC, HSYNC} during blanking
    input            de,         // data enable (active video)
    output reg [9:0] tmds        // 10-bit TMDS symbol
);

    // ================================================================
    // First encoding stage: compute q_m[8:0]
    // ================================================================
    wire [3:0] n1_d;    // number of 1s in data[7:0]
    wire       xor_decision;
    wire [7:0] q_m_data;  // encoded data bits [7:0]
    wire       q_m_bit8;  // encoding type flag

    // Count 1s in input data
    assign n1_d = data[0] + data[1] + data[2] + data[3] +
                  data[4] + data[5] + data[6] + data[7];

    // Decision: XOR when n1_d > 4 or (n1_d == 4 and data[0] == 0)
    assign xor_decision = (n1_d > 4) || ((n1_d == 4) && (data[0] == 1'b0));

    // q_m[0] is always data[0]
    assign q_m_data[0] = data[0];

    // XOR or XNOR encoding for bits 1-7
    assign q_m_data[1] = xor_decision ? (q_m_data[0] ^ data[1]) : (q_m_data[0] ~^ data[1]);
    assign q_m_data[2] = xor_decision ? (q_m_data[1] ^ data[2]) : (q_m_data[1] ~^ data[2]);
    assign q_m_data[3] = xor_decision ? (q_m_data[2] ^ data[3]) : (q_m_data[2] ~^ data[3]);
    assign q_m_data[4] = xor_decision ? (q_m_data[3] ^ data[4]) : (q_m_data[3] ~^ data[4]);
    assign q_m_data[5] = xor_decision ? (q_m_data[4] ^ data[5]) : (q_m_data[4] ~^ data[5]);
    assign q_m_data[6] = xor_decision ? (q_m_data[5] ^ data[6]) : (q_m_data[5] ~^ data[6]);
    assign q_m_data[7] = xor_decision ? (q_m_data[6] ^ data[7]) : (q_m_data[6] ~^ data[7]);

    // q_m[8] is the encoding type flag (0 = XOR, 1 = XNOR)
    assign q_m_bit8 = ~xor_decision;

    // ================================================================
    // Count 1s and 0s in q_m_data
    // ================================================================
    wire [3:0] n1_qm;   // number of 1s in q_m_data
    wire signed [4:0] n0_qm;  // number of 0s in q_m_data = 8 - n1_qm
    wire signed [4:0] diff_qm; // n1 - n0 difference

    assign n1_qm = q_m_data[0] + q_m_data[1] + q_m_data[2] + q_m_data[3] +
                   q_m_data[4] + q_m_data[5] + q_m_data[6] + q_m_data[7];

    assign diff_qm = {1'b0, n1_qm} - {1'b0, (4'd8 - n1_qm)};

    // ================================================================
    // Running disparity counter (signed)
    // ================================================================
    reg signed [4:0] cnt;  // positive = more 1s sent

    // ================================================================
    // Control code lookup (blanking)
    // ================================================================
    wire [9:0] control_code;
    assign control_code = (ctrl == 2'b00) ? 10'b11_0101_0100 :
                          (ctrl == 2'b01) ? 10'b00_1010_1011 :
                          (ctrl == 2'b10) ? 10'b01_0101_0100 :
                                             10'b10_1010_1011;

    // ================================================================
    // Main encoder state machine
    // ================================================================
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            cnt  <= 5'd0;
            tmds <= 10'b11_0101_0100;
        end else begin
            if (!de) begin
                // Blanking: output control code, reset disparity
                cnt  <= 5'd0;
                tmds <= control_code;
            end else begin
                // Active video: 8b/10b encode
                if ((cnt == 0) || (diff_qm == 0)) begin
                    // cnt is zero or data is perfectly balanced
                    if (q_m_bit8) begin
                        // XNOR encoding: send as-is with bit9=0
                        tmds <= {1'b0, q_m_bit8, q_m_data};
                        cnt  <= cnt + diff_qm;
                    end else begin
                        // XOR encoding: invert and set bit9=1
                        tmds <= {1'b1, q_m_bit8, ~q_m_data};
                        cnt  <= cnt - diff_qm;
                    end
                end else begin
                    // cnt != 0 and diff_qm != 0
                    // Check if inverting would reduce disparity
                    if (((cnt > 0) && (diff_qm > 0)) || ((cnt < 0) && (diff_qm < 0))) begin
                        // Invert to oppose disparity
                        tmds <= {1'b1, q_m_bit8, ~q_m_data};
                        if (q_m_bit8)
                            cnt <= cnt - diff_qm;
                        else
                            cnt <= cnt - diff_qm + 5'sd2;
                    end else begin
                        // Don't invert
                        tmds <= {1'b0, q_m_bit8, q_m_data};
                        if (q_m_bit8)
                            cnt <= cnt + diff_qm + 5'sd2;
                        else
                            cnt <= cnt + diff_qm;
                    end
                end
            end
        end
    end

endmodule
