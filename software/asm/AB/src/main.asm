  ORG 0x0000

main:
  LD    sp, 0xFFFF

loop:
  INC   A              ; 3C — 1 byte
  LD    (0x0400), A    ; 32 00 04 — 3 bytes
  out   (0x80), A
  JP    loop           ; C3 03 00 — 3 bytes (absolute jump to 0x0003)
