    ORG     0xD400
    LD      SP, 0xFFFF
    LD      HL, msg
    CALL    puts
    HALT
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
msg:
    DEFB    "CCP OK AT D400", 0x0D, 0x0A, 0
