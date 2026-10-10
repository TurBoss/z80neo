// ============================================================================
// Font ROM - 8x16 pixel monospace font (ASCII 0-127)
// Based on classic IBM VGA font (Code Page 437)
// ============================================================================
//
// Memory layout:
//   - 128 characters x 16 rows = 2048 bytes
//   - addr[10:4] = character code (7 bits)
//   - addr[3:0]  = row within character (4 bits)
//   - Each row is 8 bits (one pixel per bit, MSB leftmost)

module font_rom (
    input            clk,
    input      [10:0] addr,       // {char_code[6:0], row[3:0]}
    output reg [7:0] data         // 8-bit row bitmap
);

    // Font data stored as a packed 2048-byte array
    // 128 chars * 16 rows = 2048 bytes
    reg [7:0] font [0:2047];

    // Initialize font data using readmemh-like approach
    // Using initial block for simulation; synthesis reads from BRAM init
    // For open-source toolchain, we initialize inline

    // Character 0x00 (NUL) - Empty (used as space)
    // Character 0x01 (SOH)
    // ...
    // Character 0x20 (' ') = space
    // Character 0x41 ('A')
    // etc.

    // This is a subset of the font. For full font, we'd need 2048 bytes.
    // We provide the most common printable characters and fill the rest with blanks.

    // We'll use a generate-like approach but simpler: a big case statement
    // in an always block for BRAM initialization.

    integer i;
    initial begin
        // Initialize all to zero first
        for (i = 0; i < 2048; i = i + 1) begin
            font[i] = 8'h00;
        end

        // Space (0x20)
        font[16*8'h20 +  0] = 8'h00;
        font[16*8'h20 +  1] = 8'h00;
        font[16*8'h20 +  2] = 8'h00;
        font[16*8'h20 +  3] = 8'h00;
        font[16*8'h20 +  4] = 8'h00;
        font[16*8'h20 +  5] = 8'h00;
        font[16*8'h20 +  6] = 8'h00;
        font[16*8'h20 +  7] = 8'h00;
        font[16*8'h20 +  8] = 8'h00;
        font[16*8'h20 +  9] = 8'h00;
        font[16*8'h20 + 10] = 8'h00;
        font[16*8'h20 + 11] = 8'h00;
        font[16*8'h20 + 12] = 8'h00;
        font[16*8'h20 + 13] = 8'h00;
        font[16*8'h20 + 14] = 8'h00;
        font[16*8'h20 + 15] = 8'h00;

        // Exclamation mark '!' (0x21)
        font[16*8'h21 +  0] = 8'h00;
        font[16*8'h21 +  1] = 8'h18;
        font[16*8'h21 +  2] = 8'h3C;
        font[16*8'h21 +  3] = 8'h3C;
        font[16*8'h21 +  4] = 8'h3C;
        font[16*8'h21 +  5] = 8'h18;
        font[16*8'h21 +  6] = 8'h18;
        font[16*8'h21 +  7] = 8'h18;
        font[16*8'h21 +  8] = 8'h00;
        font[16*8'h21 +  9] = 8'h18;
        font[16*8'h21 + 10] = 8'h18;
        font[16*8'h21 + 11] = 8'h00;
        font[16*8'h21 + 12] = 8'h00;
        font[16*8'h21 + 13] = 8'h00;
        font[16*8'h21 + 14] = 8'h00;
        font[16*8'h21 + 15] = 8'h00;

        // Double quote '"' (0x22)
        font[16*8'h22 +  0] = 8'h00;
        font[16*8'h22 +  1] = 8'h66;
        font[16*8'h22 +  2] = 8'h66;
        font[16*8'h22 +  3] = 8'h66;
        font[16*8'h22 +  4] = 8'h24;
        font[16*8'h22 +  5] = 8'h00;
        font[16*8'h22 +  6] = 8'h00;
        font[16*8'h22 +  7] = 8'h00;
        font[16*8'h22 +  8] = 8'h00;
        font[16*8'h22 +  9] = 8'h00;
        font[16*8'h22 + 10] = 8'h00;
        font[16*8'h22 + 11] = 8'h00;
        font[16*8'h22 + 12] = 8'h00;
        font[16*8'h22 + 13] = 8'h00;
        font[16*8'h22 + 14] = 8'h00;
        font[16*8'h22 + 15] = 8'h00;

        // Hash '#' (0x23)
        font[16*8'h23 +  0] = 8'h00;
        font[16*8'h23 +  1] = 8'h6C;
        font[16*8'h23 +  2] = 8'h6C;
        font[16*8'h23 +  3] = 8'hFE;
        font[16*8'h23 +  4] = 8'h6C;
        font[16*8'h23 +  5] = 8'h6C;
        font[16*8'h23 +  6] = 8'h6C;
        font[16*8'h23 +  7] = 8'hFE;
        font[16*8'h23 +  8] = 8'h6C;
        font[16*8'h23 +  9] = 8'h6C;
        font[16*8'h23 + 10] = 8'h00;
        font[16*8'h23 + 11] = 8'h00;
        font[16*8'h23 + 12] = 8'h00;
        font[16*8'h23 + 13] = 8'h00;
        font[16*8'h23 + 14] = 8'h00;
        font[16*8'h23 + 15] = 8'h00;

        // Dollar '$' (0x24)
        font[16*8'h24 +  0] = 8'h10;
        font[16*8'h24 +  1] = 8'h7C;
        font[16*8'h24 +  2] = 8'hD6;
        font[16*8'h24 +  3] = 8'hD0;
        font[16*8'h24 +  4] = 8'hD0;
        font[16*8'h24 +  5] = 8'h7C;
        font[16*8'h24 +  6] = 8'h16;
        font[16*8'h24 +  7] = 8'h16;
        font[16*8'h24 +  8] = 8'hD6;
        font[16*8'h24 +  9] = 8'h7C;
        font[16*8'h24 + 10] = 8'h10;
        font[16*8'h24 + 11] = 8'h00;
        font[16*8'h24 + 12] = 8'h00;
        font[16*8'h24 + 13] = 8'h00;
        font[16*8'h24 + 14] = 8'h00;
        font[16*8'h24 + 15] = 8'h00;

        // Percent '%' (0x25)
        font[16*8'h25 +  0] = 8'h00;
        font[16*8'h25 +  1] = 8'h00;
        font[16*8'h25 +  2] = 8'h62;
        font[16*8'h25 +  3] = 8'h66;
        font[16*8'h25 +  4] = 8'h0C;
        font[16*8'h25 +  5] = 8'h18;
        font[16*8'h25 +  6] = 8'h30;
        font[16*8'h25 +  7] = 8'h66;
        font[16*8'h25 +  8] = 8'h46;
        font[16*8'h25 +  9] = 8'h00;
        font[16*8'h25 + 10] = 8'h00;
        font[16*8'h25 + 11] = 8'h00;
        font[16*8'h25 + 12] = 8'h00;
        font[16*8'h25 + 13] = 8'h00;
        font[16*8'h25 + 14] = 8'h00;
        font[16*8'h25 + 15] = 8'h00;

        // Ampersand '&' (0x26)
        font[16*8'h26 +  0] = 8'h00;
        font[16*8'h26 +  1] = 8'h38;
        font[16*8'h26 +  2] = 8'h6C;
        font[16*8'h26 +  3] = 8'h6C;
        font[16*8'h26 +  4] = 8'h38;
        font[16*8'h26 +  5] = 8'h76;
        font[16*8'h26 +  6] = 8'hDC;
        font[16*8'h26 +  7] = 8'hCC;
        font[16*8'h26 +  8] = 8'hCC;
        font[16*8'h26 +  9] = 8'h76;
        font[16*8'h26 + 10] = 8'h00;
        font[16*8'h26 + 11] = 8'h00;
        font[16*8'h26 + 12] = 8'h00;
        font[16*8'h26 + 13] = 8'h00;
        font[16*8'h26 + 14] = 8'h00;
        font[16*8'h26 + 15] = 8'h00;

        // Quote (0x27)
        font[16*8'h27 +  0] = 8'h00;
        font[16*8'h27 +  1] = 8'h30;
        font[16*8'h27 +  2] = 8'h30;
        font[16*8'h27 +  3] = 8'h30;
        font[16*8'h27 +  4] = 8'h60;
        font[16*8'h27 +  5] = 8'h00;
        font[16*8'h27 +  6] = 8'h00;
        font[16*8'h27 +  7] = 8'h00;
        font[16*8'h27 +  8] = 8'h00;
        font[16*8'h27 +  9] = 8'h00;
        font[16*8'h27 + 10] = 8'h00;
        font[16*8'h27 + 11] = 8'h00;
        font[16*8'h27 + 12] = 8'h00;
        font[16*8'h27 + 13] = 8'h00;
        font[16*8'h27 + 14] = 8'h00;
        font[16*8'h27 + 15] = 8'h00;

        // Left paren '(' (0x28)
        font[16*8'h28 +  0] = 8'h00;
        font[16*8'h28 +  1] = 8'h0C;
        font[16*8'h28 +  2] = 8'h18;
        font[16*8'h28 +  3] = 8'h30;
        font[16*8'h28 +  4] = 8'h30;
        font[16*8'h28 +  5] = 8'h30;
        font[16*8'h28 +  6] = 8'h30;
        font[16*8'h28 +  7] = 8'h30;
        font[16*8'h28 +  8] = 8'h18;
        font[16*8'h28 +  9] = 8'h0C;
        font[16*8'h28 + 10] = 8'h00;
        font[16*8'h28 + 11] = 8'h00;
        font[16*8'h28 + 12] = 8'h00;
        font[16*8'h28 + 13] = 8'h00;
        font[16*8'h28 + 14] = 8'h00;
        font[16*8'h28 + 15] = 8'h00;

        // Right paren ')' (0x29)
        font[16*8'h29 +  0] = 8'h00;
        font[16*8'h29 +  1] = 8'h30;
        font[16*8'h29 +  2] = 8'h18;
        font[16*8'h29 +  3] = 8'h0C;
        font[16*8'h29 +  4] = 8'h0C;
        font[16*8'h29 +  5] = 8'h0C;
        font[16*8'h29 +  6] = 8'h0C;
        font[16*8'h29 +  7] = 8'h0C;
        font[16*8'h29 +  8] = 8'h18;
        font[16*8'h29 +  9] = 8'h30;
        font[16*8'h29 + 10] = 8'h00;
        font[16*8'h29 + 11] = 8'h00;
        font[16*8'h29 + 12] = 8'h00;
        font[16*8'h29 + 13] = 8'h00;
        font[16*8'h29 + 14] = 8'h00;
        font[16*8'h29 + 15] = 8'h00;

        // Asterisk '*' (0x2A)
        font[16*8'h2A +  0] = 8'h00;
        font[16*8'h2A +  1] = 8'h00;
        font[16*8'h2A +  2] = 8'h00;
        font[16*8'h2A +  3] = 8'h10;
        font[16*8'h2A +  4] = 8'hD6;
        font[16*8'h2A +  5] = 8'h7C;
        font[16*8'h2A +  6] = 8'h38;
        font[16*8'h2A +  7] = 8'h7C;
        font[16*8'h2A +  8] = 8'hD6;
        font[16*8'h2A +  9] = 8'h10;
        font[16*8'h2A + 10] = 8'h00;
        font[16*8'h2A + 11] = 8'h00;
        font[16*8'h2A + 12] = 8'h00;
        font[16*8'h2A + 13] = 8'h00;
        font[16*8'h2A + 14] = 8'h00;
        font[16*8'h2A + 15] = 8'h00;

        // Plus '+' (0x2B)
        font[16*8'h2B +  0] = 8'h00;
        font[16*8'h2B +  1] = 8'h00;
        font[16*8'h2B +  2] = 8'h00;
        font[16*8'h2B +  3] = 8'h10;
        font[16*8'h2B +  4] = 8'h10;
        font[16*8'h2B +  5] = 8'h7C;
        font[16*8'h2B +  6] = 8'h10;
        font[16*8'h2B +  7] = 8'h10;
        font[16*8'h2B +  8] = 8'h00;
        font[16*8'h2B +  9] = 8'h00;
        font[16*8'h2B + 10] = 8'h00;
        font[16*8'h2B + 11] = 8'h00;
        font[16*8'h2B + 12] = 8'h00;
        font[16*8'h2B + 13] = 8'h00;
        font[16*8'h2B + 14] = 8'h00;
        font[16*8'h2B + 15] = 8'h00;

        // Comma ',' (0x2C)
        font[16*8'h2C +  0] = 8'h00;
        font[16*8'h2C +  1] = 8'h00;
        font[16*8'h2C +  2] = 8'h00;
        font[16*8'h2C +  3] = 8'h00;
        font[16*8'h2C +  4] = 8'h00;
        font[16*8'h2C +  5] = 8'h00;
        font[16*8'h2C +  6] = 8'h00;
        font[16*8'h2C +  7] = 8'h00;
        font[16*8'h2C +  8] = 8'h18;
        font[16*8'h2C +  9] = 8'h18;
        font[16*8'h2C + 10] = 8'h08;
        font[16*8'h2C + 11] = 8'h10;
        font[16*8'h2C + 12] = 8'h00;
        font[16*8'h2C + 13] = 8'h00;
        font[16*8'h2C + 14] = 8'h00;
        font[16*8'h2C + 15] = 8'h00;

        // Dash '-' (0x2D)
        font[16*8'h2D +  0] = 8'h00;
        font[16*8'h2D +  1] = 8'h00;
        font[16*8'h2D +  2] = 8'h00;
        font[16*8'h2D +  3] = 8'h00;
        font[16*8'h2D +  4] = 8'h00;
        font[16*8'h2D +  5] = 8'h7E;
        font[16*8'h2D +  6] = 8'h00;
        font[16*8'h2D +  7] = 8'h00;
        font[16*8'h2D +  8] = 8'h00;
        font[16*8'h2D +  9] = 8'h00;
        font[16*8'h2D + 10] = 8'h00;
        font[16*8'h2D + 11] = 8'h00;
        font[16*8'h2D + 12] = 8'h00;
        font[16*8'h2D + 13] = 8'h00;
        font[16*8'h2D + 14] = 8'h00;
        font[16*8'h2D + 15] = 8'h00;

        // Period '.' (0x2E)
        font[16*8'h2E +  0] = 8'h00;
        font[16*8'h2E +  1] = 8'h00;
        font[16*8'h2E +  2] = 8'h00;
        font[16*8'h2E +  3] = 8'h00;
        font[16*8'h2E +  4] = 8'h00;
        font[16*8'h2E +  5] = 8'h00;
        font[16*8'h2E +  6] = 8'h00;
        font[16*8'h2E +  7] = 8'h00;
        font[16*8'h2E +  8] = 8'h18;
        font[16*8'h2E +  9] = 8'h18;
        font[16*8'h2E + 10] = 8'h00;
        font[16*8'h2E + 11] = 8'h00;
        font[16*8'h2E + 12] = 8'h00;
        font[16*8'h2E + 13] = 8'h00;
        font[16*8'h2E + 14] = 8'h00;
        font[16*8'h2E + 15] = 8'h00;

        // Slash '/' (0x2F)
        font[16*8'h2F +  0] = 8'h00;
        font[16*8'h2F +  1] = 8'h06;
        font[16*8'h2F +  2] = 8'h06;
        font[16*8'h2F +  3] = 8'h0C;
        font[16*8'h2F +  4] = 8'h0C;
        font[16*8'h2F +  5] = 8'h18;
        font[16*8'h2F +  6] = 8'h18;
        font[16*8'h2F +  7] = 8'h30;
        font[16*8'h2F +  8] = 8'h30;
        font[16*8'h2F +  9] = 8'h60;
        font[16*8'h2F + 10] = 8'h60;
        font[16*8'h2F + 11] = 8'h00;
        font[16*8'h2F + 12] = 8'h00;
        font[16*8'h2F + 13] = 8'h00;
        font[16*8'h2F + 14] = 8'h00;
        font[16*8'h2F + 15] = 8'h00;

        // Digit '0' (0x30)
        font[16*8'h30 +  0] = 8'h00;
        font[16*8'h30 +  1] = 8'h3C;
        font[16*8'h30 +  2] = 8'h66;
        font[16*8'h30 +  3] = 8'h6E;
        font[16*8'h30 +  4] = 8'h76;
        font[16*8'h30 +  5] = 8'h66;
        font[16*8'h30 +  6] = 8'h66;
        font[16*8'h30 +  7] = 8'h6E;
        font[16*8'h30 +  8] = 8'h66;
        font[16*8'h30 +  9] = 8'h3C;
        font[16*8'h30 + 10] = 8'h00;
        font[16*8'h30 + 11] = 8'h00;
        font[16*8'h30 + 12] = 8'h00;
        font[16*8'h30 + 13] = 8'h00;
        font[16*8'h30 + 14] = 8'h00;
        font[16*8'h30 + 15] = 8'h00;

        // Digit '1' (0x31)
        font[16*8'h31 +  0] = 8'h00;
        font[16*8'h31 +  1] = 8'h18;
        font[16*8'h31 +  2] = 8'h38;
        font[16*8'h31 +  3] = 8'h78;
        font[16*8'h31 +  4] = 8'h18;
        font[16*8'h31 +  5] = 8'h18;
        font[16*8'h31 +  6] = 8'h18;
        font[16*8'h31 +  7] = 8'h18;
        font[16*8'h31 +  8] = 8'h18;
        font[16*8'h31 +  9] = 8'h7E;
        font[16*8'h31 + 10] = 8'h00;
        font[16*8'h31 + 11] = 8'h00;
        font[16*8'h31 + 12] = 8'h00;
        font[16*8'h31 + 13] = 8'h00;
        font[16*8'h31 + 14] = 8'h00;
        font[16*8'h31 + 15] = 8'h00;

        // Digit '2' (0x32)
        font[16*8'h32 +  0] = 8'h00;
        font[16*8'h32 +  1] = 8'h3C;
        font[16*8'h32 +  2] = 8'h66;
        font[16*8'h32 +  3] = 8'h06;
        font[16*8'h32 +  4] = 8'h0C;
        font[16*8'h32 +  5] = 8'h18;
        font[16*8'h32 +  6] = 8'h30;
        font[16*8'h32 +  7] = 8'h60;
        font[16*8'h32 +  8] = 8'h66;
        font[16*8'h32 +  9] = 8'hFE;
        font[16*8'h32 + 10] = 8'h00;
        font[16*8'h32 + 11] = 8'h00;
        font[16*8'h32 + 12] = 8'h00;
        font[16*8'h32 + 13] = 8'h00;
        font[16*8'h32 + 14] = 8'h00;
        font[16*8'h32 + 15] = 8'h00;

        // Digit '3' (0x33)
        font[16*8'h33 +  0] = 8'h00;
        font[16*8'h33 +  1] = 8'h3C;
        font[16*8'h33 +  2] = 8'h66;
        font[16*8'h33 +  3] = 8'h06;
        font[16*8'h33 +  4] = 8'h06;
        font[16*8'h33 +  5] = 8'h1C;
        font[16*8'h33 +  6] = 8'h06;
        font[16*8'h33 +  7] = 8'h06;
        font[16*8'h33 +  8] = 8'h66;
        font[16*8'h33 +  9] = 8'h3C;
        font[16*8'h33 + 10] = 8'h00;
        font[16*8'h33 + 11] = 8'h00;
        font[16*8'h33 + 12] = 8'h00;
        font[16*8'h33 + 13] = 8'h00;
        font[16*8'h33 + 14] = 8'h00;
        font[16*8'h33 + 15] = 8'h00;

        // Digit '4' (0x34)
        font[16*8'h34 +  0] = 8'h00;
        font[16*8'h34 +  1] = 8'h0C;
        font[16*8'h34 +  2] = 8'h1C;
        font[16*8'h34 +  3] = 8'h3C;
        font[16*8'h34 +  4] = 8'h6C;
        font[16*8'h34 +  5] = 8'hCC;
        font[16*8'h34 +  6] = 8'hFE;
        font[16*8'h34 +  7] = 8'h0C;
        font[16*8'h34 +  8] = 8'h0C;
        font[16*8'h34 +  9] = 8'h1E;
        font[16*8'h34 + 10] = 8'h00;
        font[16*8'h34 + 11] = 8'h00;
        font[16*8'h34 + 12] = 8'h00;
        font[16*8'h34 + 13] = 8'h00;
        font[16*8'h34 + 14] = 8'h00;
        font[16*8'h34 + 15] = 8'h00;

        // Digit '5' (0x35)
        font[16*8'h35 +  0] = 8'h00;
        font[16*8'h35 +  1] = 8'hFE;
        font[16*8'h35 +  2] = 8'hC0;
        font[16*8'h35 +  3] = 8'hC0;
        font[16*8'h35 +  4] = 8'hFC;
        font[16*8'h35 +  5] = 8'h06;
        font[16*8'h35 +  6] = 8'h06;
        font[16*8'h35 +  7] = 8'h06;
        font[16*8'h35 +  8] = 8'hC6;
        font[16*8'h35 +  9] = 8'h7C;
        font[16*8'h35 + 10] = 8'h00;
        font[16*8'h35 + 11] = 8'h00;
        font[16*8'h35 + 12] = 8'h00;
        font[16*8'h35 + 13] = 8'h00;
        font[16*8'h35 + 14] = 8'h00;
        font[16*8'h35 + 15] = 8'h00;

        // Digit '6' (0x36)
        font[16*8'h36 +  0] = 8'h00;
        font[16*8'h36 +  1] = 8'h38;
        font[16*8'h36 +  2] = 8'h60;
        font[16*8'h36 +  3] = 8'hC0;
        font[16*8'h36 +  4] = 8'hFC;
        font[16*8'h36 +  5] = 8'hC6;
        font[16*8'h36 +  6] = 8'hC6;
        font[16*8'h36 +  7] = 8'hC6;
        font[16*8'h36 +  8] = 8'hC6;
        font[16*8'h36 +  9] = 8'h7C;
        font[16*8'h36 + 10] = 8'h00;
        font[16*8'h36 + 11] = 8'h00;
        font[16*8'h36 + 12] = 8'h00;
        font[16*8'h36 + 13] = 8'h00;
        font[16*8'h36 + 14] = 8'h00;
        font[16*8'h36 + 15] = 8'h00;

        // Digit '7' (0x37)
        font[16*8'h37 +  0] = 8'h00;
        font[16*8'h37 +  1] = 8'hFE;
        font[16*8'h37 +  2] = 8'hC6;
        font[16*8'h37 +  3] = 8'h06;
        font[16*8'h37 +  4] = 8'h0C;
        font[16*8'h37 +  5] = 8'h18;
        font[16*8'h37 +  6] = 8'h30;
        font[16*8'h37 +  7] = 8'h30;
        font[16*8'h37 +  8] = 8'h30;
        font[16*8'h37 +  9] = 8'h30;
        font[16*8'h37 + 10] = 8'h00;
        font[16*8'h37 + 11] = 8'h00;
        font[16*8'h37 + 12] = 8'h00;
        font[16*8'h37 + 13] = 8'h00;
        font[16*8'h37 + 14] = 8'h00;
        font[16*8'h37 + 15] = 8'h00;

        // Digit '8' (0x38)
        font[16*8'h38 +  0] = 8'h00;
        font[16*8'h38 +  1] = 8'h7C;
        font[16*8'h38 +  2] = 8'hC6;
        font[16*8'h38 +  3] = 8'hC6;
        font[16*8'h38 +  4] = 8'hC6;
        font[16*8'h38 +  5] = 8'h7C;
        font[16*8'h38 +  6] = 8'hC6;
        font[16*8'h38 +  7] = 8'hC6;
        font[16*8'h38 +  8] = 8'hC6;
        font[16*8'h38 +  9] = 8'h7C;
        font[16*8'h38 + 10] = 8'h00;
        font[16*8'h38 + 11] = 8'h00;
        font[16*8'h38 + 12] = 8'h00;
        font[16*8'h38 + 13] = 8'h00;
        font[16*8'h38 + 14] = 8'h00;
        font[16*8'h38 + 15] = 8'h00;

        // Digit '9' (0x39)
        font[16*8'h39 +  0] = 8'h00;
        font[16*8'h39 +  1] = 8'h7C;
        font[16*8'h39 +  2] = 8'hC6;
        font[16*8'h39 +  3] = 8'hC6;
        font[16*8'h39 +  4] = 8'hC6;
        font[16*8'h39 +  5] = 8'h7E;
        font[16*8'h39 +  6] = 8'h06;
        font[16*8'h39 +  7] = 8'h06;
        font[16*8'h39 +  8] = 8'h0C;
        font[16*8'h39 +  9] = 8'h78;
        font[16*8'h39 + 10] = 8'h00;
        font[16*8'h39 + 11] = 8'h00;
        font[16*8'h39 + 12] = 8'h00;
        font[16*8'h39 + 13] = 8'h00;
        font[16*8'h39 + 14] = 8'h00;
        font[16*8'h39 + 15] = 8'h00;

        // Colon ':' (0x3A)
        font[16*8'h3A +  0] = 8'h00;
        font[16*8'h3A +  1] = 8'h00;
        font[16*8'h3A +  2] = 8'h00;
        font[16*8'h3A +  3] = 8'h18;
        font[16*8'h3A +  4] = 8'h18;
        font[16*8'h3A +  5] = 8'h00;
        font[16*8'h3A +  6] = 8'h00;
        font[16*8'h3A +  7] = 8'h18;
        font[16*8'h3A +  8] = 8'h18;
        font[16*8'h3A +  9] = 8'h00;
        font[16*8'h3A + 10] = 8'h00;
        font[16*8'h3A + 11] = 8'h00;
        font[16*8'h3A + 12] = 8'h00;
        font[16*8'h3A + 13] = 8'h00;
        font[16*8'h3A + 14] = 8'h00;
        font[16*8'h3A + 15] = 8'h00;

        // Semicolon ';' (0x3B)
        font[16*8'h3B +  0] = 8'h00;
        font[16*8'h3B +  1] = 8'h00;
        font[16*8'h3B +  2] = 8'h00;
        font[16*8'h3B +  3] = 8'h18;
        font[16*8'h3B +  4] = 8'h18;
        font[16*8'h3B +  5] = 8'h00;
        font[16*8'h3B +  6] = 8'h00;
        font[16*8'h3B +  7] = 8'h18;
        font[16*8'h3B +  8] = 8'h18;
        font[16*8'h3B +  9] = 8'h08;
        font[16*8'h3B + 10] = 8'h10;
        font[16*8'h3B + 11] = 8'h00;
        font[16*8'h3B + 12] = 8'h00;
        font[16*8'h3B + 13] = 8'h00;
        font[16*8'h3B + 14] = 8'h00;
        font[16*8'h3B + 15] = 8'h00;

        // Less '<' (0x3C)
        font[16*8'h3C +  0] = 8'h00;
        font[16*8'h3C +  1] = 8'h0C;
        font[16*8'h3C +  2] = 8'h18;
        font[16*8'h3C +  3] = 8'h30;
        font[16*8'h3C +  4] = 8'h60;
        font[16*8'h3C +  5] = 8'h30;
        font[16*8'h3C +  6] = 8'h18;
        font[16*8'h3C +  7] = 8'h0C;
        font[16*8'h3C +  8] = 8'h00;
        font[16*8'h3C +  9] = 8'h00;
        font[16*8'h3C + 10] = 8'h00;
        font[16*8'h3C + 11] = 8'h00;
        font[16*8'h3C + 12] = 8'h00;
        font[16*8'h3C + 13] = 8'h00;
        font[16*8'h3C + 14] = 8'h00;
        font[16*8'h3C + 15] = 8'h00;

        // Equals '=' (0x3D)
        font[16*8'h3D +  0] = 8'h00;
        font[16*8'h3D +  1] = 8'h00;
        font[16*8'h3D +  2] = 8'h00;
        font[16*8'h3D +  3] = 8'h00;
        font[16*8'h3D +  4] = 8'h7E;
        font[16*8'h3D +  5] = 8'h00;
        font[16*8'h3D +  6] = 8'h00;
        font[16*8'h3D +  7] = 8'h7E;
        font[16*8'h3D +  8] = 8'h00;
        font[16*8'h3D +  9] = 8'h00;
        font[16*8'h3D + 10] = 8'h00;
        font[16*8'h3D + 11] = 8'h00;
        font[16*8'h3D + 12] = 8'h00;
        font[16*8'h3D + 13] = 8'h00;
        font[16*8'h3D + 14] = 8'h00;
        font[16*8'h3D + 15] = 8'h00;

        // Greater '>' (0x3E)
        font[16*8'h3E +  0] = 8'h00;
        font[16*8'h3E +  1] = 8'h60;
        font[16*8'h3E +  2] = 8'h30;
        font[16*8'h3E +  3] = 8'h18;
        font[16*8'h3E +  4] = 8'h0C;
        font[16*8'h3E +  5] = 8'h18;
        font[16*8'h3E +  6] = 8'h30;
        font[16*8'h3E +  7] = 8'h60;
        font[16*8'h3E +  8] = 8'h00;
        font[16*8'h3E +  9] = 8'h00;
        font[16*8'h3E + 10] = 8'h00;
        font[16*8'h3E + 11] = 8'h00;
        font[16*8'h3E + 12] = 8'h00;
        font[16*8'h3E + 13] = 8'h00;
        font[16*8'h3E + 14] = 8'h00;
        font[16*8'h3E + 15] = 8'h00;

        // Question '?' (0x3F)
        font[16*8'h3F +  0] = 8'h00;
        font[16*8'h3F +  1] = 8'h3C;
        font[16*8'h3F +  2] = 8'h66;
        font[16*8'h3F +  3] = 8'h06;
        font[16*8'h3F +  4] = 8'h0C;
        font[16*8'h3F +  5] = 8'h18;
        font[16*8'h3F +  6] = 8'h18;
        font[16*8'h3F +  7] = 8'h00;
        font[16*8'h3F +  8] = 8'h18;
        font[16*8'h3F +  9] = 8'h18;
        font[16*8'h3F + 10] = 8'h00;
        font[16*8'h3F + 11] = 8'h00;
        font[16*8'h3F + 12] = 8'h00;
        font[16*8'h3F + 13] = 8'h00;
        font[16*8'h3F + 14] = 8'h00;
        font[16*8'h3F + 15] = 8'h00;

        // At '@' (0x40)
        font[16*8'h40 +  0] = 8'h00;
        font[16*8'h40 +  1] = 8'h7C;
        font[16*8'h40 +  2] = 8'hC6;
        font[16*8'h40 +  3] = 8'hDE;
        font[16*8'h40 +  4] = 8'hDE;
        font[16*8'h40 +  5] = 8'hDE;
        font[16*8'h40 +  6] = 8'hDC;
        font[16*8'h40 +  7] = 8'hC0;
        font[16*8'h40 +  8] = 8'hC6;
        font[16*8'h40 +  9] = 8'h7C;
        font[16*8'h40 + 10] = 8'h00;
        font[16*8'h40 + 11] = 8'h00;
        font[16*8'h40 + 12] = 8'h00;
        font[16*8'h40 + 13] = 8'h00;
        font[16*8'h40 + 14] = 8'h00;
        font[16*8'h40 + 15] = 8'h00;

        // 'A' (0x41)
        font[16*8'h41 +  0] = 8'h00;
        font[16*8'h41 +  1] = 8'h10;
        font[16*8'h41 +  2] = 8'h38;
        font[16*8'h41 +  3] = 8'h6C;
        font[16*8'h41 +  4] = 8'hC6;
        font[16*8'h41 +  5] = 8'hC6;
        font[16*8'h41 +  6] = 8'hFE;
        font[16*8'h41 +  7] = 8'hC6;
        font[16*8'h41 +  8] = 8'hC6;
        font[16*8'h41 +  9] = 8'hC6;
        font[16*8'h41 + 10] = 8'h00;
        font[16*8'h41 + 11] = 8'h00;
        font[16*8'h41 + 12] = 8'h00;
        font[16*8'h41 + 13] = 8'h00;
        font[16*8'h41 + 14] = 8'h00;
        font[16*8'h41 + 15] = 8'h00;

        // 'B' (0x42)
        font[16*8'h42 +  0] = 8'h00;
        font[16*8'h42 +  1] = 8'hFC;
        font[16*8'h42 +  2] = 8'h66;
        font[16*8'h42 +  3] = 8'h66;
        font[16*8'h42 +  4] = 8'h66;
        font[16*8'h42 +  5] = 8'h7C;
        font[16*8'h42 +  6] = 8'h66;
        font[16*8'h42 +  7] = 8'h66;
        font[16*8'h42 +  8] = 8'h66;
        font[16*8'h42 +  9] = 8'hFC;
        font[16*8'h42 + 10] = 8'h00;
        font[16*8'h42 + 11] = 8'h00;
        font[16*8'h42 + 12] = 8'h00;
        font[16*8'h42 + 13] = 8'h00;
        font[16*8'h42 + 14] = 8'h00;
        font[16*8'h42 + 15] = 8'h00;

        // 'C' (0x43)
        font[16*8'h43 +  0] = 8'h00;
        font[16*8'h43 +  1] = 8'h3C;
        font[16*8'h43 +  2] = 8'h66;
        font[16*8'h43 +  3] = 8'hC0;
        font[16*8'h43 +  4] = 8'hC0;
        font[16*8'h43 +  5] = 8'hC0;
        font[16*8'h43 +  6] = 8'hC0;
        font[16*8'h43 +  7] = 8'hC0;
        font[16*8'h43 +  8] = 8'h66;
        font[16*8'h43 +  9] = 8'h3C;
        font[16*8'h43 + 10] = 8'h00;
        font[16*8'h43 + 11] = 8'h00;
        font[16*8'h43 + 12] = 8'h00;
        font[16*8'h43 + 13] = 8'h00;
        font[16*8'h43 + 14] = 8'h00;
        font[16*8'h43 + 15] = 8'h00;

        // 'D' (0x44)
        font[16*8'h44 +  0] = 8'h00;
        font[16*8'h44 +  1] = 8'hF8;
        font[16*8'h44 +  2] = 8'h6C;
        font[16*8'h44 +  3] = 8'h66;
        font[16*8'h44 +  4] = 8'h66;
        font[16*8'h44 +  5] = 8'h66;
        font[16*8'h44 +  6] = 8'h66;
        font[16*8'h44 +  7] = 8'h66;
        font[16*8'h44 +  8] = 8'h6C;
        font[16*8'h44 +  9] = 8'hF8;
        font[16*8'h44 + 10] = 8'h00;
        font[16*8'h44 + 11] = 8'h00;
        font[16*8'h44 + 12] = 8'h00;
        font[16*8'h44 + 13] = 8'h00;
        font[16*8'h44 + 14] = 8'h00;
        font[16*8'h44 + 15] = 8'h00;

        // 'E' (0x45)
        font[16*8'h45 +  0] = 8'h00;
        font[16*8'h45 +  1] = 8'hFE;
        font[16*8'h45 +  2] = 8'h62;
        font[16*8'h45 +  3] = 8'h68;
        font[16*8'h45 +  4] = 8'h68;
        font[16*8'h45 +  5] = 8'h78;
        font[16*8'h45 +  6] = 8'h68;
        font[16*8'h45 +  7] = 8'h60;
        font[16*8'h45 +  8] = 8'h62;
        font[16*8'h45 +  9] = 8'hFE;
        font[16*8'h45 + 10] = 8'h00;
        font[16*8'h45 + 11] = 8'h00;
        font[16*8'h45 + 12] = 8'h00;
        font[16*8'h45 + 13] = 8'h00;
        font[16*8'h45 + 14] = 8'h00;
        font[16*8'h45 + 15] = 8'h00;

        // 'F' (0x46)
        font[16*8'h46 +  0] = 8'h00;
        font[16*8'h46 +  1] = 8'hFE;
        font[16*8'h46 +  2] = 8'h62;
        font[16*8'h46 +  3] = 8'h68;
        font[16*8'h46 +  4] = 8'h68;
        font[16*8'h46 +  5] = 8'h78;
        font[16*8'h46 +  6] = 8'h68;
        font[16*8'h46 +  7] = 8'h60;
        font[16*8'h46 +  8] = 8'h60;
        font[16*8'h46 +  9] = 8'hF0;
        font[16*8'h46 + 10] = 8'h00;
        font[16*8'h46 + 11] = 8'h00;
        font[16*8'h46 + 12] = 8'h00;
        font[16*8'h46 + 13] = 8'h00;
        font[16*8'h46 + 14] = 8'h00;
        font[16*8'h46 + 15] = 8'h00;

        // 'G' (0x47)
        font[16*8'h47 +  0] = 8'h00;
        font[16*8'h47 +  1] = 8'h3C;
        font[16*8'h47 +  2] = 8'h66;
        font[16*8'h47 +  3] = 8'hC0;
        font[16*8'h47 +  4] = 8'hC0;
        font[16*8'h47 +  5] = 8'hC0;
        font[16*8'h47 +  6] = 8'hCE;
        font[16*8'h47 +  7] = 8'hC6;
        font[16*8'h47 +  8] = 8'h66;
        font[16*8'h47 +  9] = 8'h3E;
        font[16*8'h47 + 10] = 8'h00;
        font[16*8'h47 + 11] = 8'h00;
        font[16*8'h47 + 12] = 8'h00;
        font[16*8'h47 + 13] = 8'h00;
        font[16*8'h47 + 14] = 8'h00;
        font[16*8'h47 + 15] = 8'h00;

        // 'H' (0x48)
        font[16*8'h48 +  0] = 8'h00;
        font[16*8'h48 +  1] = 8'hC6;
        font[16*8'h48 +  2] = 8'hC6;
        font[16*8'h48 +  3] = 8'hC6;
        font[16*8'h48 +  4] = 8'hC6;
        font[16*8'h48 +  5] = 8'hFE;
        font[16*8'h48 +  6] = 8'hC6;
        font[16*8'h48 +  7] = 8'hC6;
        font[16*8'h48 +  8] = 8'hC6;
        font[16*8'h48 +  9] = 8'hC6;
        font[16*8'h48 + 10] = 8'h00;
        font[16*8'h48 + 11] = 8'h00;
        font[16*8'h48 + 12] = 8'h00;
        font[16*8'h48 + 13] = 8'h00;
        font[16*8'h48 + 14] = 8'h00;
        font[16*8'h48 + 15] = 8'h00;

        // 'I' (0x49)
        font[16*8'h49 +  0] = 8'h00;
        font[16*8'h49 +  1] = 8'h7E;
        font[16*8'h49 +  2] = 8'h18;
        font[16*8'h49 +  3] = 8'h18;
        font[16*8'h49 +  4] = 8'h18;
        font[16*8'h49 +  5] = 8'h18;
        font[16*8'h49 +  6] = 8'h18;
        font[16*8'h49 +  7] = 8'h18;
        font[16*8'h49 +  8] = 8'h18;
        font[16*8'h49 +  9] = 8'h7E;
        font[16*8'h49 + 10] = 8'h00;
        font[16*8'h49 + 11] = 8'h00;
        font[16*8'h49 + 12] = 8'h00;
        font[16*8'h49 + 13] = 8'h00;
        font[16*8'h49 + 14] = 8'h00;
        font[16*8'h49 + 15] = 8'h00;

        // 'J' (0x4A)
        font[16*8'h4A +  0] = 8'h00;
        font[16*8'h4A +  1] = 8'h1E;
        font[16*8'h4A +  2] = 8'h0C;
        font[16*8'h4A +  3] = 8'h0C;
        font[16*8'h4A +  4] = 8'h0C;
        font[16*8'h4A +  5] = 8'h0C;
        font[16*8'h4A +  6] = 8'h0C;
        font[16*8'h4A +  7] = 8'hCC;
        font[16*8'h4A +  8] = 8'hCC;
        font[16*8'h4A +  9] = 8'h78;
        font[16*8'h4A + 10] = 8'h00;
        font[16*8'h4A + 11] = 8'h00;
        font[16*8'h4A + 12] = 8'h00;
        font[16*8'h4A + 13] = 8'h00;
        font[16*8'h4A + 14] = 8'h00;
        font[16*8'h4A + 15] = 8'h00;

        // 'K' (0x4B)
        font[16*8'h4B +  0] = 8'h00;
        font[16*8'h4B +  1] = 8'hE6;
        font[16*8'h4B +  2] = 8'h66;
        font[16*8'h4B +  3] = 8'h6C;
        font[16*8'h4B +  4] = 8'h6C;
        font[16*8'h4B +  5] = 8'h78;
        font[16*8'h4B +  6] = 8'h6C;
        font[16*8'h4B +  7] = 8'h6C;
        font[16*8'h4B +  8] = 8'h66;
        font[16*8'h4B +  9] = 8'hE6;
        font[16*8'h4B + 10] = 8'h00;
        font[16*8'h4B + 11] = 8'h00;
        font[16*8'h4B + 12] = 8'h00;
        font[16*8'h4B + 13] = 8'h00;
        font[16*8'h4B + 14] = 8'h00;
        font[16*8'h4B + 15] = 8'h00;

        // 'L' (0x4C)
        font[16*8'h4C +  0] = 8'h00;
        font[16*8'h4C +  1] = 8'hF0;
        font[16*8'h4C +  2] = 8'h60;
        font[16*8'h4C +  3] = 8'h60;
        font[16*8'h4C +  4] = 8'h60;
        font[16*8'h4C +  5] = 8'h60;
        font[16*8'h4C +  6] = 8'h60;
        font[16*8'h4C +  7] = 8'h62;
        font[16*8'h4C +  8] = 8'h66;
        font[16*8'h4C +  9] = 8'hFE;
        font[16*8'h4C + 10] = 8'h00;
        font[16*8'h4C + 11] = 8'h00;
        font[16*8'h4C + 12] = 8'h00;
        font[16*8'h4C + 13] = 8'h00;
        font[16*8'h4C + 14] = 8'h00;
        font[16*8'h4C + 15] = 8'h00;

        // 'M' (0x4D)
        font[16*8'h4D +  0] = 8'h00;
        font[16*8'h4D +  1] = 8'hC6;
        font[16*8'h4D +  2] = 8'hEE;
        font[16*8'h4D +  3] = 8'hFE;
        font[16*8'h4D +  4] = 8'hFE;
        font[16*8'h4D +  5] = 8'hD6;
        font[16*8'h4D +  6] = 8'hC6;
        font[16*8'h4D +  7] = 8'hC6;
        font[16*8'h4D +  8] = 8'hC6;
        font[16*8'h4D +  9] = 8'hC6;
        font[16*8'h4D + 10] = 8'h00;
        font[16*8'h4D + 11] = 8'h00;
        font[16*8'h4D + 12] = 8'h00;
        font[16*8'h4D + 13] = 8'h00;
        font[16*8'h4D + 14] = 8'h00;
        font[16*8'h4D + 15] = 8'h00;

        // 'N' (0x4E)
        font[16*8'h4E +  0] = 8'h00;
        font[16*8'h4E +  1] = 8'hC6;
        font[16*8'h4E +  2] = 8'hE6;
        font[16*8'h4E +  3] = 8'hF6;
        font[16*8'h4E +  4] = 8'hFE;
        font[16*8'h4E +  5] = 8'hDE;
        font[16*8'h4E +  6] = 8'hCE;
        font[16*8'h4E +  7] = 8'hC6;
        font[16*8'h4E +  8] = 8'hC6;
        font[16*8'h4E +  9] = 8'hC6;
        font[16*8'h4E + 10] = 8'h00;
        font[16*8'h4E + 11] = 8'h00;
        font[16*8'h4E + 12] = 8'h00;
        font[16*8'h4E + 13] = 8'h00;
        font[16*8'h4E + 14] = 8'h00;
        font[16*8'h4E + 15] = 8'h00;

        // 'O' (0x4F)
        font[16*8'h4F +  0] = 8'h00;
        font[16*8'h4F +  1] = 8'h7C;
        font[16*8'h4F +  2] = 8'hC6;
        font[16*8'h4F +  3] = 8'hC6;
        font[16*8'h4F +  4] = 8'hC6;
        font[16*8'h4F +  5] = 8'hC6;
        font[16*8'h4F +  6] = 8'hC6;
        font[16*8'h4F +  7] = 8'hC6;
        font[16*8'h4F +  8] = 8'hC6;
        font[16*8'h4F +  9] = 8'h7C;
        font[16*8'h4F + 10] = 8'h00;
        font[16*8'h4F + 11] = 8'h00;
        font[16*8'h4F + 12] = 8'h00;
        font[16*8'h4F + 13] = 8'h00;
        font[16*8'h4F + 14] = 8'h00;
        font[16*8'h4F + 15] = 8'h00;

        // 'P' (0x50)
        font[16*8'h50 +  0] = 8'h00;
        font[16*8'h50 +  1] = 8'hFC;
        font[16*8'h50 +  2] = 8'h66;
        font[16*8'h50 +  3] = 8'h66;
        font[16*8'h50 +  4] = 8'h66;
        font[16*8'h50 +  5] = 8'h7C;
        font[16*8'h50 +  6] = 8'h60;
        font[16*8'h50 +  7] = 8'h60;
        font[16*8'h50 +  8] = 8'h60;
        font[16*8'h50 +  9] = 8'hF0;
        font[16*8'h50 + 10] = 8'h00;
        font[16*8'h50 + 11] = 8'h00;
        font[16*8'h50 + 12] = 8'h00;
        font[16*8'h50 + 13] = 8'h00;
        font[16*8'h50 + 14] = 8'h00;
        font[16*8'h50 + 15] = 8'h00;

        // 'Q' (0x51)
        font[16*8'h51 +  0] = 8'h00;
        font[16*8'h51 +  1] = 8'h7C;
        font[16*8'h51 +  2] = 8'hC6;
        font[16*8'h51 +  3] = 8'hC6;
        font[16*8'h51 +  4] = 8'hC6;
        font[16*8'h51 +  5] = 8'hC6;
        font[16*8'h51 +  6] = 8'hC6;
        font[16*8'h51 +  7] = 8'hD6;
        font[16*8'h51 +  8] = 8'hDE;
        font[16*8'h51 +  9] = 8'h7C;
        font[16*8'h51 + 10] = 8'h0C;
        font[16*8'h51 + 11] = 8'h0E;
        font[16*8'h51 + 12] = 8'h00;
        font[16*8'h51 + 13] = 8'h00;
        font[16*8'h51 + 14] = 8'h00;
        font[16*8'h51 + 15] = 8'h00;

        // 'R' (0x52)
        font[16*8'h52 +  0] = 8'h00;
        font[16*8'h52 +  1] = 8'hFC;
        font[16*8'h52 +  2] = 8'h66;
        font[16*8'h52 +  3] = 8'h66;
        font[16*8'h52 +  4] = 8'h66;
        font[16*8'h52 +  5] = 8'h7C;
        font[16*8'h52 +  6] = 8'h6C;
        font[16*8'h52 +  7] = 8'h66;
        font[16*8'h52 +  8] = 8'h66;
        font[16*8'h52 +  9] = 8'hE6;
        font[16*8'h52 + 10] = 8'h00;
        font[16*8'h52 + 11] = 8'h00;
        font[16*8'h52 + 12] = 8'h00;
        font[16*8'h52 + 13] = 8'h00;
        font[16*8'h52 + 14] = 8'h00;
        font[16*8'h52 + 15] = 8'h00;

        // 'S' (0x53)
        font[16*8'h53 +  0] = 8'h00;
        font[16*8'h53 +  1] = 8'h7C;
        font[16*8'h53 +  2] = 8'hC6;
        font[16*8'h53 +  3] = 8'hC6;
        font[16*8'h53 +  4] = 8'h60;
        font[16*8'h53 +  5] = 8'h38;
        font[16*8'h53 +  6] = 8'h0C;
        font[16*8'h53 +  7] = 8'h06;
        font[16*8'h53 +  8] = 8'hC6;
        font[16*8'h53 +  9] = 8'h7C;
        font[16*8'h53 + 10] = 8'h00;
        font[16*8'h53 + 11] = 8'h00;
        font[16*8'h53 + 12] = 8'h00;
        font[16*8'h53 + 13] = 8'h00;
        font[16*8'h53 + 14] = 8'h00;
        font[16*8'h53 + 15] = 8'h00;

        // 'T' (0x54)
        font[16*8'h54 +  0] = 8'h00;
        font[16*8'h54 +  1] = 8'h7E;
        font[16*8'h54 +  2] = 8'h5A;
        font[16*8'h54 +  3] = 8'h18;
        font[16*8'h54 +  4] = 8'h18;
        font[16*8'h54 +  5] = 8'h18;
        font[16*8'h54 +  6] = 8'h18;
        font[16*8'h54 +  7] = 8'h18;
        font[16*8'h54 +  8] = 8'h18;
        font[16*8'h54 +  9] = 8'h3C;
        font[16*8'h54 + 10] = 8'h00;
        font[16*8'h54 + 11] = 8'h00;
        font[16*8'h54 + 12] = 8'h00;
        font[16*8'h54 + 13] = 8'h00;
        font[16*8'h54 + 14] = 8'h00;
        font[16*8'h54 + 15] = 8'h00;

        // 'U' (0x55)
        font[16*8'h55 +  0] = 8'h00;
        font[16*8'h55 +  1] = 8'hC6;
        font[16*8'h55 +  2] = 8'hC6;
        font[16*8'h55 +  3] = 8'hC6;
        font[16*8'h55 +  4] = 8'hC6;
        font[16*8'h55 +  5] = 8'hC6;
        font[16*8'h55 +  6] = 8'hC6;
        font[16*8'h55 +  7] = 8'hC6;
        font[16*8'h55 +  8] = 8'hC6;
        font[16*8'h55 +  9] = 8'h7C;
        font[16*8'h55 + 10] = 8'h00;
        font[16*8'h55 + 11] = 8'h00;
        font[16*8'h55 + 12] = 8'h00;
        font[16*8'h55 + 13] = 8'h00;
        font[16*8'h55 + 14] = 8'h00;
        font[16*8'h55 + 15] = 8'h00;

        // 'V' (0x56)
        font[16*8'h56 +  0] = 8'h00;
        font[16*8'h56 +  1] = 8'hC6;
        font[16*8'h56 +  2] = 8'hC6;
        font[16*8'h56 +  3] = 8'hC6;
        font[16*8'h56 +  4] = 8'hC6;
        font[16*8'h56 +  5] = 8'hC6;
        font[16*8'h56 +  6] = 8'hC6;
        font[16*8'h56 +  7] = 8'hC6;
        font[16*8'h56 +  8] = 8'h7C;
        font[16*8'h56 +  9] = 8'h38;
        font[16*8'h56 + 10] = 8'h00;
        font[16*8'h56 + 11] = 8'h00;
        font[16*8'h56 + 12] = 8'h00;
        font[16*8'h56 + 13] = 8'h00;
        font[16*8'h56 + 14] = 8'h00;
        font[16*8'h56 + 15] = 8'h00;

        // 'W' (0x57)
        font[16*8'h57 +  0] = 8'h00;
        font[16*8'h57 +  1] = 8'hC6;
        font[16*8'h57 +  2] = 8'hC6;
        font[16*8'h57 +  3] = 8'hC6;
        font[16*8'h57 +  4] = 8'hC6;
        font[16*8'h57 +  5] = 8'hD6;
        font[16*8'h57 +  6] = 8'hD6;
        font[16*8'h57 +  7] = 8'hD6;
        font[16*8'h57 +  8] = 8'hFE;
        font[16*8'h57 +  9] = 8'h6C;
        font[16*8'h57 + 10] = 8'h00;
        font[16*8'h57 + 11] = 8'h00;
        font[16*8'h57 + 12] = 8'h00;
        font[16*8'h57 + 13] = 8'h00;
        font[16*8'h57 + 14] = 8'h00;
        font[16*8'h57 + 15] = 8'h00;

        // 'X' (0x58)
        font[16*8'h58 +  0] = 8'h00;
        font[16*8'h58 +  1] = 8'hC6;
        font[16*8'h58 +  2] = 8'hC6;
        font[16*8'h58 +  3] = 8'h6C;
        font[16*8'h58 +  4] = 8'h6C;
        font[16*8'h58 +  5] = 8'h38;
        font[16*8'h58 +  6] = 8'h38;
        font[16*8'h58 +  7] = 8'h6C;
        font[16*8'h58 +  8] = 8'h6C;
        font[16*8'h58 +  9] = 8'hC6;
        font[16*8'h58 + 10] = 8'h00;
        font[16*8'h58 + 11] = 8'h00;
        font[16*8'h58 + 12] = 8'h00;
        font[16*8'h58 + 13] = 8'h00;
        font[16*8'h58 + 14] = 8'h00;
        font[16*8'h58 + 15] = 8'h00;

        // 'Y' (0x59)
        font[16*8'h59 +  0] = 8'h00;
        font[16*8'h59 +  1] = 8'h66;
        font[16*8'h59 +  2] = 8'h66;
        font[16*8'h59 +  3] = 8'h66;
        font[16*8'h59 +  4] = 8'h66;
        font[16*8'h59 +  5] = 8'h3C;
        font[16*8'h59 +  6] = 8'h18;
        font[16*8'h59 +  7] = 8'h18;
        font[16*8'h59 +  8] = 8'h18;
        font[16*8'h59 +  9] = 8'h3C;
        font[16*8'h59 + 10] = 8'h00;
        font[16*8'h59 + 11] = 8'h00;
        font[16*8'h59 + 12] = 8'h00;
        font[16*8'h59 + 13] = 8'h00;
        font[16*8'h59 + 14] = 8'h00;
        font[16*8'h59 + 15] = 8'h00;

        // 'Z' (0x5A)
        font[16*8'h5A +  0] = 8'h00;
        font[16*8'h5A +  1] = 8'hFE;
        font[16*8'h5A +  2] = 8'hC6;
        font[16*8'h5A +  3] = 8'h8C;
        font[16*8'h5A +  4] = 8'h18;
        font[16*8'h5A +  5] = 8'h18;
        font[16*8'h5A +  6] = 8'h30;
        font[16*8'h5A +  7] = 8'h32;
        font[16*8'h5A +  8] = 8'h66;
        font[16*8'h5A +  9] = 8'hFE;
        font[16*8'h5A + 10] = 8'h00;
        font[16*8'h5A + 11] = 8'h00;
        font[16*8'h5A + 12] = 8'h00;
        font[16*8'h5A + 13] = 8'h00;
        font[16*8'h5A + 14] = 8'h00;
        font[16*8'h5A + 15] = 8'h00;

        // Left bracket '[' (0x5B)
        font[16*8'h5B +  0] = 8'h00;
        font[16*8'h5B +  1] = 8'h3C;
        font[16*8'h5B +  2] = 8'h30;
        font[16*8'h5B +  3] = 8'h30;
        font[16*8'h5B +  4] = 8'h30;
        font[16*8'h5B +  5] = 8'h30;
        font[16*8'h5B +  6] = 8'h30;
        font[16*8'h5B +  7] = 8'h30;
        font[16*8'h5B +  8] = 8'h30;
        font[16*8'h5B +  9] = 8'h3C;
        font[16*8'h5B + 10] = 8'h00;
        font[16*8'h5B + 11] = 8'h00;
        font[16*8'h5B + 12] = 8'h00;
        font[16*8'h5B + 13] = 8'h00;
        font[16*8'h5B + 14] = 8'h00;
        font[16*8'h5B + 15] = 8'h00;

        // Backslash '\' (0x5C)
        font[16*8'h5C +  0] = 8'h00;
        font[16*8'h5C +  1] = 8'h60;
        font[16*8'h5C +  2] = 8'h60;
        font[16*8'h5C +  3] = 8'h30;
        font[16*8'h5C +  4] = 8'h30;
        font[16*8'h5C +  5] = 8'h18;
        font[16*8'h5C +  6] = 8'h18;
        font[16*8'h5C +  7] = 8'h0C;
        font[16*8'h5C +  8] = 8'h0C;
        font[16*8'h5C +  9] = 8'h06;
        font[16*8'h5C + 10] = 8'h06;
        font[16*8'h5C + 11] = 8'h00;
        font[16*8'h5C + 12] = 8'h00;
        font[16*8'h5C + 13] = 8'h00;
        font[16*8'h5C + 14] = 8'h00;
        font[16*8'h5C + 15] = 8'h00;

        // Right bracket ']' (0x5D)
        font[16*8'h5D +  0] = 8'h00;
        font[16*8'h5D +  1] = 8'h3C;
        font[16*8'h5D +  2] = 8'h0C;
        font[16*8'h5D +  3] = 8'h0C;
        font[16*8'h5D +  4] = 8'h0C;
        font[16*8'h5D +  5] = 8'h0C;
        font[16*8'h5D +  6] = 8'h0C;
        font[16*8'h5D +  7] = 8'h0C;
        font[16*8'h5D +  8] = 8'h0C;
        font[16*8'h5D +  9] = 8'h3C;
        font[16*8'h5D + 10] = 8'h00;
        font[16*8'h5D + 11] = 8'h00;
        font[16*8'h5D + 12] = 8'h00;
        font[16*8'h5D + 13] = 8'h00;
        font[16*8'h5D + 14] = 8'h00;
        font[16*8'h5D + 15] = 8'h00;

        // Caret '^' (0x5E)
        font[16*8'h5E +  0] = 8'h10;
        font[16*8'h5E +  1] = 8'h38;
        font[16*8'h5E +  2] = 8'h6C;
        font[16*8'h5E +  3] = 8'hC6;
        font[16*8'h5E +  4] = 8'h00;
        font[16*8'h5E +  5] = 8'h00;
        font[16*8'h5E +  6] = 8'h00;
        font[16*8'h5E +  7] = 8'h00;
        font[16*8'h5E +  8] = 8'h00;
        font[16*8'h5E +  9] = 8'h00;
        font[16*8'h5E + 10] = 8'h00;
        font[16*8'h5E + 11] = 8'h00;
        font[16*8'h5E + 12] = 8'h00;
        font[16*8'h5E + 13] = 8'h00;
        font[16*8'h5E + 14] = 8'h00;
        font[16*8'h5E + 15] = 8'h00;

        // Underscore '_' (0x5F)
        font[16*8'h5F +  0] = 8'h00;
        font[16*8'h5F +  1] = 8'h00;
        font[16*8'h5F +  2] = 8'h00;
        font[16*8'h5F +  3] = 8'h00;
        font[16*8'h5F +  4] = 8'h00;
        font[16*8'h5F +  5] = 8'h00;
        font[16*8'h5F +  6] = 8'h00;
        font[16*8'h5F +  7] = 8'h00;
        font[16*8'h5F +  8] = 8'h00;
        font[16*8'h5F +  9] = 8'h00;
        font[16*8'h5F + 10] = 8'h00;
        font[16*8'h5F + 11] = 8'h00;
        font[16*8'h5F + 12] = 8'hFF;
        font[16*8'h5F + 13] = 8'h00;
        font[16*8'h5F + 14] = 8'h00;
        font[16*8'h5F + 15] = 8'h00;

        // Backtick '`' (0x60)
        font[16*8'h60 +  0] = 8'h30;
        font[16*8'h60 +  1] = 8'h30;
        font[16*8'h60 +  2] = 8'h18;
        font[16*8'h60 +  3] = 8'h00;
        font[16*8'h60 +  4] = 8'h00;
        font[16*8'h60 +  5] = 8'h00;
        font[16*8'h60 +  6] = 8'h00;
        font[16*8'h60 +  7] = 8'h00;
        font[16*8'h60 +  8] = 8'h00;
        font[16*8'h60 +  9] = 8'h00;
        font[16*8'h60 + 10] = 8'h00;
        font[16*8'h60 + 11] = 8'h00;
        font[16*8'h60 + 12] = 8'h00;
        font[16*8'h60 + 13] = 8'h00;
        font[16*8'h60 + 14] = 8'h00;
        font[16*8'h60 + 15] = 8'h00;

        // 'a' (0x61)
        font[16*8'h61 +  0] = 8'h00;
        font[16*8'h61 +  1] = 8'h00;
        font[16*8'h61 +  2] = 8'h00;
        font[16*8'h61 +  3] = 8'h78;
        font[16*8'h61 +  4] = 8'h0C;
        font[16*8'h61 +  5] = 8'h7C;
        font[16*8'h61 +  6] = 8'hCC;
        font[16*8'h61 +  7] = 8'hCC;
        font[16*8'h61 +  8] = 8'hCC;
        font[16*8'h61 +  9] = 8'h76;
        font[16*8'h61 + 10] = 8'h00;
        font[16*8'h61 + 11] = 8'h00;
        font[16*8'h61 + 12] = 8'h00;
        font[16*8'h61 + 13] = 8'h00;
        font[16*8'h61 + 14] = 8'h00;
        font[16*8'h61 + 15] = 8'h00;

        // 'b' (0x62)
        font[16*8'h62 +  0] = 8'h00;
        font[16*8'h62 +  1] = 8'hE0;
        font[16*8'h62 +  2] = 8'h60;
        font[16*8'h62 +  3] = 8'h60;
        font[16*8'h62 +  4] = 8'h7C;
        font[16*8'h62 +  5] = 8'h66;
        font[16*8'h62 +  6] = 8'h66;
        font[16*8'h62 +  7] = 8'h66;
        font[16*8'h62 +  8] = 8'h66;
        font[16*8'h62 +  9] = 8'h7C;
        font[16*8'h62 + 10] = 8'h00;
        font[16*8'h62 + 11] = 8'h00;
        font[16*8'h62 + 12] = 8'h00;
        font[16*8'h62 + 13] = 8'h00;
        font[16*8'h62 + 14] = 8'h00;
        font[16*8'h62 + 15] = 8'h00;

        // 'c' (0x63)
        font[16*8'h63 +  0] = 8'h00;
        font[16*8'h63 +  1] = 8'h00;
        font[16*8'h63 +  2] = 8'h00;
        font[16*8'h63 +  3] = 8'h3C;
        font[16*8'h63 +  4] = 8'h66;
        font[16*8'h63 +  5] = 8'hC0;
        font[16*8'h63 +  6] = 8'hC0;
        font[16*8'h63 +  7] = 8'hC0;
        font[16*8'h63 +  8] = 8'h66;
        font[16*8'h63 +  9] = 8'h3C;
        font[16*8'h63 + 10] = 8'h00;
        font[16*8'h63 + 11] = 8'h00;
        font[16*8'h63 + 12] = 8'h00;
        font[16*8'h63 + 13] = 8'h00;
        font[16*8'h63 + 14] = 8'h00;
        font[16*8'h63 + 15] = 8'h00;

        // 'd' (0x64)
        font[16*8'h64 +  0] = 8'h00;
        font[16*8'h64 +  1] = 8'h1C;
        font[16*8'h64 +  2] = 8'h0C;
        font[16*8'h64 +  3] = 8'h0C;
        font[16*8'h64 +  4] = 8'h7C;
        font[16*8'h64 +  5] = 8'hCC;
        font[16*8'h64 +  6] = 8'hCC;
        font[16*8'h64 +  7] = 8'hCC;
        font[16*8'h64 +  8] = 8'hCC;
        font[16*8'h64 +  9] = 8'h76;
        font[16*8'h64 + 10] = 8'h00;
        font[16*8'h64 + 11] = 8'h00;
        font[16*8'h64 + 12] = 8'h00;
        font[16*8'h64 + 13] = 8'h00;
        font[16*8'h64 + 14] = 8'h00;
        font[16*8'h64 + 15] = 8'h00;

        // 'e' (0x65)
        font[16*8'h65 +  0] = 8'h00;
        font[16*8'h65 +  1] = 8'h00;
        font[16*8'h65 +  2] = 8'h00;
        font[16*8'h65 +  3] = 8'h7C;
        font[16*8'h65 +  4] = 8'hC6;
        font[16*8'h65 +  5] = 8'hFE;
        font[16*8'h65 +  6] = 8'hC0;
        font[16*8'h65 +  7] = 8'hC0;
        font[16*8'h65 +  8] = 8'hC6;
        font[16*8'h65 +  9] = 8'h7C;
        font[16*8'h65 + 10] = 8'h00;
        font[16*8'h65 + 11] = 8'h00;
        font[16*8'h65 + 12] = 8'h00;
        font[16*8'h65 + 13] = 8'h00;
        font[16*8'h65 + 14] = 8'h00;
        font[16*8'h65 + 15] = 8'h00;

        // 'f' (0x66)
        font[16*8'h66 +  0] = 8'h00;
        font[16*8'h66 +  1] = 8'h1E;
        font[16*8'h66 +  2] = 8'h30;
        font[16*8'h66 +  3] = 8'h30;
        font[16*8'h66 +  4] = 8'hFC;
        font[16*8'h66 +  5] = 8'h30;
        font[16*8'h66 +  6] = 8'h30;
        font[16*8'h66 +  7] = 8'h30;
        font[16*8'h66 +  8] = 8'h30;
        font[16*8'h66 +  9] = 8'h78;
        font[16*8'h66 + 10] = 8'h00;
        font[16*8'h66 + 11] = 8'h00;
        font[16*8'h66 + 12] = 8'h00;
        font[16*8'h66 + 13] = 8'h00;
        font[16*8'h66 + 14] = 8'h00;
        font[16*8'h66 + 15] = 8'h00;

        // 'g' (0x67)
        font[16*8'h67 +  0] = 8'h00;
        font[16*8'h67 +  1] = 8'h00;
        font[16*8'h67 +  2] = 8'h00;
        font[16*8'h67 +  3] = 8'h76;
        font[16*8'h67 +  4] = 8'hCC;
        font[16*8'h67 +  5] = 8'hCC;
        font[16*8'h67 +  6] = 8'hCC;
        font[16*8'h67 +  7] = 8'h7C;
        font[16*8'h67 +  8] = 8'h0C;
        font[16*8'h67 +  9] = 8'h78;
        font[16*8'h67 + 10] = 8'h00;
        font[16*8'h67 + 11] = 8'h00;
        font[16*8'h67 + 12] = 8'h00;
        font[16*8'h67 + 13] = 8'h00;
        font[16*8'h67 + 14] = 8'h00;
        font[16*8'h67 + 15] = 8'h00;

        // 'h' (0x68)
        font[16*8'h68 +  0] = 8'h00;
        font[16*8'h68 +  1] = 8'hE0;
        font[16*8'h68 +  2] = 8'h60;
        font[16*8'h68 +  3] = 8'h60;
        font[16*8'h68 +  4] = 8'h7C;
        font[16*8'h68 +  5] = 8'h66;
        font[16*8'h68 +  6] = 8'h66;
        font[16*8'h68 +  7] = 8'h66;
        font[16*8'h68 +  8] = 8'h66;
        font[16*8'h68 +  9] = 8'hE6;
        font[16*8'h68 + 10] = 8'h00;
        font[16*8'h68 + 11] = 8'h00;
        font[16*8'h68 + 12] = 8'h00;
        font[16*8'h68 + 13] = 8'h00;
        font[16*8'h68 + 14] = 8'h00;
        font[16*8'h68 + 15] = 8'h00;

        // 'i' (0x69)
        font[16*8'h69 +  0] = 8'h00;
        font[16*8'h69 +  1] = 8'h18;
        font[16*8'h69 +  2] = 8'h18;
        font[16*8'h69 +  3] = 8'h00;
        font[16*8'h69 +  4] = 8'h38;
        font[16*8'h69 +  5] = 8'h18;
        font[16*8'h69 +  6] = 8'h18;
        font[16*8'h69 +  7] = 8'h18;
        font[16*8'h69 +  8] = 8'h18;
        font[16*8'h69 +  9] = 8'h3C;
        font[16*8'h69 + 10] = 8'h00;
        font[16*8'h69 + 11] = 8'h00;
        font[16*8'h69 + 12] = 8'h00;
        font[16*8'h69 + 13] = 8'h00;
        font[16*8'h69 + 14] = 8'h00;
        font[16*8'h69 + 15] = 8'h00;

        // 'j' (0x6A)
        font[16*8'h6A +  0] = 8'h00;
        font[16*8'h6A +  1] = 8'h06;
        font[16*8'h6A +  2] = 8'h06;
        font[16*8'h6A +  3] = 8'h00;
        font[16*8'h6A +  4] = 8'h0E;
        font[16*8'h6A +  5] = 8'h06;
        font[16*8'h6A +  6] = 8'h06;
        font[16*8'h6A +  7] = 8'h06;
        font[16*8'h6A +  8] = 8'h66;
        font[16*8'h6A +  9] = 8'h3C;
        font[16*8'h6A + 10] = 8'h00;
        font[16*8'h6A + 11] = 8'h00;
        font[16*8'h6A + 12] = 8'h00;
        font[16*8'h6A + 13] = 8'h00;
        font[16*8'h6A + 14] = 8'h00;
        font[16*8'h6A + 15] = 8'h00;

        // 'k' (0x6B)
        font[16*8'h6B +  0] = 8'h00;
        font[16*8'h6B +  1] = 8'hE0;
        font[16*8'h6B +  2] = 8'h60;
        font[16*8'h6B +  3] = 8'h66;
        font[16*8'h6B +  4] = 8'h6C;
        font[16*8'h6B +  5] = 8'h78;
        font[16*8'h6B +  6] = 8'h78;
        font[16*8'h6B +  7] = 8'h6C;
        font[16*8'h6B +  8] = 8'h66;
        font[16*8'h6B +  9] = 8'hE6;
        font[16*8'h6B + 10] = 8'h00;
        font[16*8'h6B + 11] = 8'h00;
        font[16*8'h6B + 12] = 8'h00;
        font[16*8'h6B + 13] = 8'h00;
        font[16*8'h6B + 14] = 8'h00;
        font[16*8'h6B + 15] = 8'h00;

        // 'l' (0x6C)
        font[16*8'h6C +  0] = 8'h00;
        font[16*8'h6C +  1] = 8'h38;
        font[16*8'h6C +  2] = 8'h18;
        font[16*8'h6C +  3] = 8'h18;
        font[16*8'h6C +  4] = 8'h18;
        font[16*8'h6C +  5] = 8'h18;
        font[16*8'h6C +  6] = 8'h18;
        font[16*8'h6C +  7] = 8'h18;
        font[16*8'h6C +  8] = 8'h18;
        font[16*8'h6C +  9] = 8'h3C;
        font[16*8'h6C + 10] = 8'h00;
        font[16*8'h6C + 11] = 8'h00;
        font[16*8'h6C + 12] = 8'h00;
        font[16*8'h6C + 13] = 8'h00;
        font[16*8'h6C + 14] = 8'h00;
        font[16*8'h6C + 15] = 8'h00;

        // 'm' (0x6D)
        font[16*8'h6D +  0] = 8'h00;
        font[16*8'h6D +  1] = 8'h00;
        font[16*8'h6D +  2] = 8'h00;
        font[16*8'h6D +  3] = 8'hEC;
        font[16*8'h6D +  4] = 8'hFE;
        font[16*8'h6D +  5] = 8'hD6;
        font[16*8'h6D +  6] = 8'hD6;
        font[16*8'h6D +  7] = 8'hD6;
        font[16*8'h6D +  8] = 8'hD6;
        font[16*8'h6D +  9] = 8'hC6;
        font[16*8'h6D + 10] = 8'h00;
        font[16*8'h6D + 11] = 8'h00;
        font[16*8'h6D + 12] = 8'h00;
        font[16*8'h6D + 13] = 8'h00;
        font[16*8'h6D + 14] = 8'h00;
        font[16*8'h6D + 15] = 8'h00;

        // 'n' (0x6E)
        font[16*8'h6E +  0] = 8'h00;
        font[16*8'h6E +  1] = 8'h00;
        font[16*8'h6E +  2] = 8'h00;
        font[16*8'h6E +  3] = 8'hDC;
        font[16*8'h6E +  4] = 8'h66;
        font[16*8'h6E +  5] = 8'h66;
        font[16*8'h6E +  6] = 8'h66;
        font[16*8'h6E +  7] = 8'h66;
        font[16*8'h6E +  8] = 8'h66;
        font[16*8'h6E +  9] = 8'h66;
        font[16*8'h6E + 10] = 8'h00;
        font[16*8'h6E + 11] = 8'h00;
        font[16*8'h6E + 12] = 8'h00;
        font[16*8'h6E + 13] = 8'h00;
        font[16*8'h6E + 14] = 8'h00;
        font[16*8'h6E + 15] = 8'h00;

        // 'o' (0x6F)
        font[16*8'h6F +  0] = 8'h00;
        font[16*8'h6F +  1] = 8'h00;
        font[16*8'h6F +  2] = 8'h00;
        font[16*8'h6F +  3] = 8'h7C;
        font[16*8'h6F +  4] = 8'hC6;
        font[16*8'h6F +  5] = 8'hC6;
        font[16*8'h6F +  6] = 8'hC6;
        font[16*8'h6F +  7] = 8'hC6;
        font[16*8'h6F +  8] = 8'hC6;
        font[16*8'h6F +  9] = 8'h7C;
        font[16*8'h6F + 10] = 8'h00;
        font[16*8'h6F + 11] = 8'h00;
        font[16*8'h6F + 12] = 8'h00;
        font[16*8'h6F + 13] = 8'h00;
        font[16*8'h6F + 14] = 8'h00;
        font[16*8'h6F + 15] = 8'h00;

        // 'p' (0x70)
        font[16*8'h70 +  0] = 8'h00;
        font[16*8'h70 +  1] = 8'h00;
        font[16*8'h70 +  2] = 8'h00;
        font[16*8'h70 +  3] = 8'hDC;
        font[16*8'h70 +  4] = 8'h66;
        font[16*8'h70 +  5] = 8'h66;
        font[16*8'h70 +  6] = 8'h66;
        font[16*8'h70 +  7] = 8'h7C;
        font[16*8'h70 +  8] = 8'h60;
        font[16*8'h70 +  9] = 8'hF0;
        font[16*8'h70 + 10] = 8'h00;
        font[16*8'h70 + 11] = 8'h00;
        font[16*8'h70 + 12] = 8'h00;
        font[16*8'h70 + 13] = 8'h00;
        font[16*8'h70 + 14] = 8'h00;
        font[16*8'h70 + 15] = 8'h00;

        // 'q' (0x71)
        font[16*8'h71 +  0] = 8'h00;
        font[16*8'h71 +  1] = 8'h00;
        font[16*8'h71 +  2] = 8'h00;
        font[16*8'h71 +  3] = 8'h76;
        font[16*8'h71 +  4] = 8'hCC;
        font[16*8'h71 +  5] = 8'hCC;
        font[16*8'h71 +  6] = 8'hCC;
        font[16*8'h71 +  7] = 8'h7C;
        font[16*8'h71 +  8] = 8'h0C;
        font[16*8'h71 +  9] = 8'h1E;
        font[16*8'h71 + 10] = 8'h00;
        font[16*8'h71 + 11] = 8'h00;
        font[16*8'h71 + 12] = 8'h00;
        font[16*8'h71 + 13] = 8'h00;
        font[16*8'h71 + 14] = 8'h00;
        font[16*8'h71 + 15] = 8'h00;

        // 'r' (0x72)
        font[16*8'h72 +  0] = 8'h00;
        font[16*8'h72 +  1] = 8'h00;
        font[16*8'h72 +  2] = 8'h00;
        font[16*8'h72 +  3] = 8'hDC;
        font[16*8'h72 +  4] = 8'h76;
        font[16*8'h72 +  5] = 8'h66;
        font[16*8'h72 +  6] = 8'h60;
        font[16*8'h72 +  7] = 8'h60;
        font[16*8'h72 +  8] = 8'h60;
        font[16*8'h72 +  9] = 8'hF0;
        font[16*8'h72 + 10] = 8'h00;
        font[16*8'h72 + 11] = 8'h00;
        font[16*8'h72 + 12] = 8'h00;
        font[16*8'h72 + 13] = 8'h00;
        font[16*8'h72 + 14] = 8'h00;
        font[16*8'h72 + 15] = 8'h00;

        // 's' (0x73)
        font[16*8'h73 +  0] = 8'h00;
        font[16*8'h73 +  1] = 8'h00;
        font[16*8'h73 +  2] = 8'h00;
        font[16*8'h73 +  3] = 8'h7C;
        font[16*8'h73 +  4] = 8'hC6;
        font[16*8'h73 +  5] = 8'h70;
        font[16*8'h73 +  6] = 8'h1C;
        font[16*8'h73 +  7] = 8'h06;
        font[16*8'h73 +  8] = 8'hC6;
        font[16*8'h73 +  9] = 8'h7C;
        font[16*8'h73 + 10] = 8'h00;
        font[16*8'h73 + 11] = 8'h00;
        font[16*8'h73 + 12] = 8'h00;
        font[16*8'h73 + 13] = 8'h00;
        font[16*8'h73 + 14] = 8'h00;
        font[16*8'h73 + 15] = 8'h00;

        // 't' (0x74)
        font[16*8'h74 +  0] = 8'h00;
        font[16*8'h74 +  1] = 8'h30;
        font[16*8'h74 +  2] = 8'h30;
        font[16*8'h74 +  3] = 8'hFC;
        font[16*8'h74 +  4] = 8'h30;
        font[16*8'h74 +  5] = 8'h30;
        font[16*8'h74 +  6] = 8'h30;
        font[16*8'h74 +  7] = 8'h30;
        font[16*8'h74 +  8] = 8'h36;
        font[16*8'h74 +  9] = 8'h1C;
        font[16*8'h74 + 10] = 8'h00;
        font[16*8'h74 + 11] = 8'h00;
        font[16*8'h74 + 12] = 8'h00;
        font[16*8'h74 + 13] = 8'h00;
        font[16*8'h74 + 14] = 8'h00;
        font[16*8'h74 + 15] = 8'h00;

        // 'u' (0x75)
        font[16*8'h75 +  0] = 8'h00;
        font[16*8'h75 +  1] = 8'h00;
        font[16*8'h75 +  2] = 8'h00;
        font[16*8'h75 +  3] = 8'hCC;
        font[16*8'h75 +  4] = 8'hCC;
        font[16*8'h75 +  5] = 8'hCC;
        font[16*8'h75 +  6] = 8'hCC;
        font[16*8'h75 +  7] = 8'hCC;
        font[16*8'h75 +  8] = 8'hCC;
        font[16*8'h75 +  9] = 8'h76;
        font[16*8'h75 + 10] = 8'h00;
        font[16*8'h75 + 11] = 8'h00;
        font[16*8'h75 + 12] = 8'h00;
        font[16*8'h75 + 13] = 8'h00;
        font[16*8'h75 + 14] = 8'h00;
        font[16*8'h75 + 15] = 8'h00;

        // 'v' (0x76)
        font[16*8'h76 +  0] = 8'h00;
        font[16*8'h76 +  1] = 8'h00;
        font[16*8'h76 +  2] = 8'h00;
        font[16*8'h76 +  3] = 8'hC6;
        font[16*8'h76 +  4] = 8'hC6;
        font[16*8'h76 +  5] = 8'hC6;
        font[16*8'h76 +  6] = 8'hC6;
        font[16*8'h76 +  7] = 8'hC6;
        font[16*8'h76 +  8] = 8'h7C;
        font[16*8'h76 +  9] = 8'h38;
        font[16*8'h76 + 10] = 8'h00;
        font[16*8'h76 + 11] = 8'h00;
        font[16*8'h76 + 12] = 8'h00;
        font[16*8'h76 + 13] = 8'h00;
        font[16*8'h76 + 14] = 8'h00;
        font[16*8'h76 + 15] = 8'h00;

        // 'w' (0x77)
        font[16*8'h77 +  0] = 8'h00;
        font[16*8'h77 +  1] = 8'h00;
        font[16*8'h77 +  2] = 8'h00;
        font[16*8'h77 +  3] = 8'hC6;
        font[16*8'h77 +  4] = 8'hC6;
        font[16*8'h77 +  5] = 8'hD6;
        font[16*8'h77 +  6] = 8'hD6;
        font[16*8'h77 +  7] = 8'hD6;
        font[16*8'h77 +  8] = 8'hFE;
        font[16*8'h77 +  9] = 8'h6C;
        font[16*8'h77 + 10] = 8'h00;
        font[16*8'h77 + 11] = 8'h00;
        font[16*8'h77 + 12] = 8'h00;
        font[16*8'h77 + 13] = 8'h00;
        font[16*8'h77 + 14] = 8'h00;
        font[16*8'h77 + 15] = 8'h00;

        // 'x' (0x78)
        font[16*8'h78 +  0] = 8'h00;
        font[16*8'h78 +  1] = 8'h00;
        font[16*8'h78 +  2] = 8'h00;
        font[16*8'h78 +  3] = 8'hC6;
        font[16*8'h78 +  4] = 8'h6C;
        font[16*8'h78 +  5] = 8'h38;
        font[16*8'h78 +  6] = 8'h38;
        font[16*8'h78 +  7] = 8'h38;
        font[16*8'h78 +  8] = 8'h6C;
        font[16*8'h78 +  9] = 8'hC6;
        font[16*8'h78 + 10] = 8'h00;
        font[16*8'h78 + 11] = 8'h00;
        font[16*8'h78 + 12] = 8'h00;
        font[16*8'h78 + 13] = 8'h00;
        font[16*8'h78 + 14] = 8'h00;
        font[16*8'h78 + 15] = 8'h00;

        // 'y' (0x79)
        font[16*8'h79 +  0] = 8'h00;
        font[16*8'h79 +  1] = 8'h00;
        font[16*8'h79 +  2] = 8'h00;
        font[16*8'h79 +  3] = 8'hC6;
        font[16*8'h79 +  4] = 8'hC6;
        font[16*8'h79 +  5] = 8'hC6;
        font[16*8'h79 +  6] = 8'hC6;
        font[16*8'h79 +  7] = 8'h7E;
        font[16*8'h79 +  8] = 8'h06;
        font[16*8'h79 +  9] = 8'h7C;
        font[16*8'h79 + 10] = 8'h00;
        font[16*8'h79 + 11] = 8'h00;
        font[16*8'h79 + 12] = 8'h00;
        font[16*8'h79 + 13] = 8'h00;
        font[16*8'h79 + 14] = 8'h00;
        font[16*8'h79 + 15] = 8'h00;

        // 'z' (0x7A)
        font[16*8'h7A +  0] = 8'h00;
        font[16*8'h7A +  1] = 8'h00;
        font[16*8'h7A +  2] = 8'h00;
        font[16*8'h7A +  3] = 8'hFE;
        font[16*8'h7A +  4] = 8'h8C;
        font[16*8'h7A +  5] = 8'h18;
        font[16*8'h7A +  6] = 8'h30;
        font[16*8'h7A +  7] = 8'h60;
        font[16*8'h7A +  8] = 8'hC2;
        font[16*8'h7A +  9] = 8'hFE;
        font[16*8'h7A + 10] = 8'h00;
        font[16*8'h7A + 11] = 8'h00;
        font[16*8'h7A + 12] = 8'h00;
        font[16*8'h7A + 13] = 8'h00;
        font[16*8'h7A + 14] = 8'h00;
        font[16*8'h7A + 15] = 8'h00;

        // '{' (0x7B)
        font[16*8'h7B +  0] = 8'h00;
        font[16*8'h7B +  1] = 8'h0E;
        font[16*8'h7B +  2] = 8'h18;
        font[16*8'h7B +  3] = 8'h18;
        font[16*8'h7B +  4] = 8'h18;
        font[16*8'h7B +  5] = 8'h70;
        font[16*8'h7B +  6] = 8'h18;
        font[16*8'h7B +  7] = 8'h18;
        font[16*8'h7B +  8] = 8'h18;
        font[16*8'h7B +  9] = 8'h0E;
        font[16*8'h7B + 10] = 8'h00;
        font[16*8'h7B + 11] = 8'h00;
        font[16*8'h7B + 12] = 8'h00;
        font[16*8'h7B + 13] = 8'h00;
        font[16*8'h7B + 14] = 8'h00;
        font[16*8'h7B + 15] = 8'h00;

        // '|' (0x7C)
        font[16*8'h7C +  0] = 8'h00;
        font[16*8'h7C +  1] = 8'h18;
        font[16*8'h7C +  2] = 8'h18;
        font[16*8'h7C +  3] = 8'h18;
        font[16*8'h7C +  4] = 8'h18;
        font[16*8'h7C +  5] = 8'h00;
        font[16*8'h7C +  6] = 8'h18;
        font[16*8'h7C +  7] = 8'h18;
        font[16*8'h7C +  8] = 8'h18;
        font[16*8'h7C +  9] = 8'h18;
        font[16*8'h7C + 10] = 8'h00;
        font[16*8'h7C + 11] = 8'h00;
        font[16*8'h7C + 12] = 8'h00;
        font[16*8'h7C + 13] = 8'h00;
        font[16*8'h7C + 14] = 8'h00;
        font[16*8'h7C + 15] = 8'h00;

        // '}' (0x7D)
        font[16*8'h7D +  0] = 8'h00;
        font[16*8'h7D +  1] = 8'h70;
        font[16*8'h7D +  2] = 8'h18;
        font[16*8'h7D +  3] = 8'h18;
        font[16*8'h7D +  4] = 8'h18;
        font[16*8'h7D +  5] = 8'h0E;
        font[16*8'h7D +  6] = 8'h18;
        font[16*8'h7D +  7] = 8'h18;
        font[16*8'h7D +  8] = 8'h18;
        font[16*8'h7D +  9] = 8'h70;
        font[16*8'h7D + 10] = 8'h00;
        font[16*8'h7D + 11] = 8'h00;
        font[16*8'h7D + 12] = 8'h00;
        font[16*8'h7D + 13] = 8'h00;
        font[16*8'h7D + 14] = 8'h00;
        font[16*8'h7D + 15] = 8'h00;

        // '~' (0x7E)
        font[16*8'h7E +  0] = 8'h00;
        font[16*8'h7E +  1] = 8'h76;
        font[16*8'h7E +  2] = 8'hDC;
        font[16*8'h7E +  3] = 8'h00;
        font[16*8'h7E +  4] = 8'h00;
        font[16*8'h7E +  5] = 8'h00;
        font[16*8'h7E +  6] = 8'h00;
        font[16*8'h7E +  7] = 8'h00;
        font[16*8'h7E +  8] = 8'h00;
        font[16*8'h7E +  9] = 8'h00;
        font[16*8'h7E + 10] = 8'h00;
        font[16*8'h7E + 11] = 8'h00;
        font[16*8'h7E + 12] = 8'h00;
        font[16*8'h7E + 13] = 8'h00;
        font[16*8'h7E + 14] = 8'h00;
        font[16*8'h7E + 15] = 8'h00;
    end

    // BRAM read: registered output
    always @(posedge clk) begin
        data <= font[addr];
    end

endmodule
