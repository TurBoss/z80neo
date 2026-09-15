    ORG     0x0000
    JP      0xEB00          ; boot vector
    DEFS    0x1700 - $
    ; BIOS at 0xEB00
    LD      SP, 0xFFFF
    LD      HL, msg1
    CALL    puts
    JP      0xD400          ; jump to CCP area
puts:
    LD      A, (HL)
    OR      A
    RET     Z
    PUSH    HL
    LD      C, A
    CALL    tx
    POP     HL
    INC     HL
    JR      puts
tx:
    IN      A, (0x81)
    AND     0x02
    JR      Z, tx
    LD      A, C
    OUT     (0x80), A
    RET
msg1:
    DEFB    "BIOS OK", 0x0D, 0x0A, 0

    ; Pad to 0xD400
    DEFS    0xD400 - $

    ; CCP at 0xD400
    LD      SP, 0xFFFE
    LD      HL, msg2
    CALL    puts
    HALT
msg2:
    DEFB    0x0D, 0x0A, 0
    DEFB    "CCP OK!!!!", 0x0D, 0x0A, 0
