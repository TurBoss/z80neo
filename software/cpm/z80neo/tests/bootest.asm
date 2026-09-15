    ORG     0
    JP      test_boot
BIOS_OFF:   EQU 1700H
    DEFS    BIOS_OFF - $
test_boot:
    LD      SP, 0EDEFH
    LD      HL, banner
    CALL    puts
    HALT
puts:
    LD      A, (HL)
    OR      A
    RET     Z
    PUSH    HL
    LD      C, A
    CALL    tx_char
    POP     HL
    INC     HL
    JR      puts
tx_char:
    IN      A, (81H)
    AND     02H
    JR      Z, tx_char
    LD      A, C
    OUT     (80H), A
    RET
banner:
    DEFB    "BOOTEST: BIOS path OK", 0DH, 0AH, 0
