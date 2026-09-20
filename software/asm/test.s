; test.s — Z80 test: outputs "Z80 OK!\r\n" in a loop
; Checks UART TX status before each byte

SERIAL_DATA:   equ 0x80
SERIAL_STATUS: equ 0x81
TX_READY:      equ 0x02

    ORG 0x0000

main:
    LD SP, 0xFFF0

loop:
    LD HL, MSG

send:
    IN A, (SERIAL_STATUS)   ; check TX ready
    AND TX_READY
    JR Z, send              ; wait until ready

    LD A, (HL)              ; get char
    OR A                    ; end of string?
    JR Z, done              ; yes

    OUT (SERIAL_DATA), A    ; send it
    INC HL                  ; next char
    JR send

done:
    ; ~1 sec delay at 150kHz
    LD DE, 4000
wait1:
    LD BC, 0
wait2:
    DEC BC
    LD A, B
    OR C
    JR NZ, wait2
    DEC DE
    LD A, D
    OR E
    JR NZ, wait1
    JP loop

MSG: DB "Z80 OK!\r\n", 0
