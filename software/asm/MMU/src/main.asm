  LD    A, 1
  OUT   (0xF0), A    ; page 0 (0x0000–0x3FFF) → bank 1
  OUT   (0xF1), A    ; page 1 (0x4000–0x7FFF) → bank 1
  IN    A, (0xF0)    ; read back what bank is mapped to page 0
