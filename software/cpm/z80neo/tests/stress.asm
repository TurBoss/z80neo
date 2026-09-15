    ORG     0x0000
    ; Boot vector
    JP      0xEB00
    ; Pad to BIOS area
    DEFS    0x1700 - $
    ; Stress test at 0xEB00
    LD      SP, 0xFFFF
loop:
    LD      A, '.'
    CALL    tx
    LD      BC, 0
wait:   DEC     BC
    LD      A, B
    OR      C
    JR      NZ, wait
    JR      loop
tx:
    PUSH    AF
tx1:    IN      A, (0x81)
    AND     0x02
    JR      Z, tx1
    POP     AF
    OUT     (0x80), A
    RET
