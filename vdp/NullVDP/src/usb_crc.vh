// ============================================================================
// usb_crc.vh - USB CRC5 (token) and CRC16 (data) as Verilog functions.
//
// Reflected (LSB-first) shift registers, matching CRC-5/USB and CRC-16/USB:
//   CRC5 : poly 0x05 reflected = 0x14, init 0x1F, final XOR 0x1F
//   CRC16: poly 0x8005 reflected = 0xA001, init 0xFFFF, final XOR 0xFFFF
// Check values over "123456789": CRC5 = 0x19, CRC16 = 0xB4C8.
//
// `include this inside a module.
// ============================================================================

// one bit, LSB-first
function [4:0] usb_crc5_step;
    input [4:0] c;
    input       b;
    begin
        usb_crc5_step = (c >> 1) ^ ((c[0] ^ b) ? 5'h14 : 5'h00);
    end
endfunction

// 11-bit token field, f[0] = first transmitted bit (addr[0],..,ep[3])
function [4:0] usb_crc5_field;
    input [10:0] f;
    integer i;
    reg [4:0] c;
    begin
        c = 5'h1F;
        for (i = 0; i < 11; i = i + 1) c = usb_crc5_step(c, f[i]);
        usb_crc5_field = c ^ 5'h1F;
    end
endfunction

// one data byte (bit0 first)
function [15:0] usb_crc16_byte;
    input [15:0] c;
    input [7:0]  d;
    integer i;
    reg [15:0] cc;
    begin
        cc = c ^ {8'h00, d};
        for (i = 0; i < 8; i = i + 1)
            cc = (cc >> 1) ^ (cc[0] ? 16'hA001 : 16'h0000);
        usb_crc16_byte = cc;
    end
endfunction

// ---------------------------------------------------------------------------
// token field helpers.  The 11-bit token field is addr[6:0] then ep[3:0],
// both transmitted LSB-first, so field[0] = addr[0] ... field[10] = ep[3].
// ---------------------------------------------------------------------------
function [10:0] usb_token_field;
    input [6:0] addr;
    input [3:0] ep;
    begin
        usb_token_field = {ep, addr};
    end
endfunction

function [4:0] usb_token_crc;
    input [6:0] addr;
    input [3:0] ep;
    begin
        usb_token_crc = usb_crc5_field({ep, addr});
    end
endfunction

// final CRC16 of a byte buffer (bytes given MSB..LSB in a 64-bit word)
function [15:0] usb_crc16_word;
    input [63:0] w;
    input [3:0]  n;          // number of bytes
    integer i;
    reg [15:0] c;
    begin
        c = 16'hFFFF;
        for (i = 0; i < 8; i = i + 1)
            if (i < n) c = usb_crc16_byte(c, w[(7-i)*8 +: 8]);
        usb_crc16_word = c ^ 16'hFFFF;
    end
endfunction
