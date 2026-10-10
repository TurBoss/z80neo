// ============================================================================
// tb_usb_crc - verify the CRC functions against CRC-5/USB and CRC-16/USB
//              catalogue check values, and the addr=0/ep=0 token CRC.
// ============================================================================
`timescale 1ns/1ps
module tb_usb_crc;
    `include "usb_crc.vh"

    reg [15:0] c16;
    reg [4:0]  c5;
    reg [10:0] field;
    integer i, errors = 0;
    integer b;
    reg [7:0] msg [0:8];

    initial begin
        // "123456789"
        msg[0]=8'h31; msg[1]=8'h32; msg[2]=8'h33; msg[3]=8'h34;
        msg[4]=8'h35; msg[5]=8'h36; msg[6]=8'h37; msg[7]=8'h38; msg[8]=8'h39;

        c16 = 16'hFFFF;
        for (i = 0; i < 9; i = i + 1) c16 = usb_crc16_byte(c16, msg[i]);
        c16 = c16 ^ 16'hFFFF;
        if (c16 !== 16'hB4C8) begin
            $display("FAIL: CRC16(\"123456789\")=0x%04X expect 0xB4C8", c16); errors=errors+1;
        end else $display("ok:   CRC16 check = 0x%04X", c16);

        // CRC5 over the byte stream "123456789" (bit LSB-first per byte)
        c5 = 5'h1F;
        for (i = 0; i < 9; i = i + 1) begin
            for (b = 0; b < 8; b = b + 1) c5 = usb_crc5_step(c5, msg[i][b]);
        end
        c5 = c5 ^ 5'h1F;
        if (c5 !== 5'h19) begin
            $display("FAIL: CRC5(\"123456789\")=0x%02X expect 0x19", c5); errors=errors+1;
        end else $display("ok:   CRC5 check  = 0x%02X", c5);

        // token CRC5 for addr=0, ep=0
        field = 11'h000;
        c5 = usb_crc5_field(field);
        if (c5 !== 5'h02) begin
            $display("FAIL: CRC5(addr0,ep0)=0x%02X expect 0x02", c5); errors=errors+1;
        end else $display("ok:   CRC5(addr0,ep0) = 0x%02X", c5);

        // token field packing check: addr=0x05, ep=0x2 -> field bits
        field = {4'h2, 7'h05};    // ep in [10:7], addr in [6:0]
        $display("info: field(addr=5,ep=2) = 0x%03X, crc5 = 0x%02X",
                 field, usb_crc5_field(field));

        if (errors == 0) $display("USB CRC TB PASSED");
        else             $display("USB CRC TB FAILED (%0d)", errors);
        $finish;
    end
endmodule
