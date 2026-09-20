; hello.asm — minimal UART test, ORG 0x4000
; Just blasts 'A' repeatedly to UART

UART_DATA equ 0x80
UART_STAT equ 0x81

    org 0x4000

start:
    ld sp, 0x7FF0
loop:
    ld a, 'A'
    call tx
    jr loop

tx:
    push af
txw:
    in a, (UART_STAT)
    and 0x02
    jr z, txw
    pop af
    out (UART_DATA), a
    ret
