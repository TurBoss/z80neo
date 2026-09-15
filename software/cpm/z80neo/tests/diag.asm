; diag.asm — minimal test at multiple addresses
    ORG     0
    ; This will be at 0x0000 (replicated to all banks)
    JP      0xEB00             ; jump to high-address version

    ; Pad so the high test lands at 0xEB00
    DEFS    0x1700 - $

    ; Test code (loaded at both 0xD400+0x1700 = 0xEB00)
    LD      SP, 0xFFFF
    LD      A, 'H'             ; "H" for high
    CALL    tx2
    LD      A, 0DH
    CALL    tx2
    LD      A, 0AH
    CALL    tx2
    HALT
tx2:
    PUSH    AF
tx2_loop:
    IN      A, (81H)
    AND     02H
    JR      Z, tx2_loop
    POP     AF
    OUT     (80H), A
    RET
