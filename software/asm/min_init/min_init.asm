; min_init.asm — minimal init.bin for testing
; Prints "INIT OK\r\n" and halts

UART_DATA   equ 0x80
UART_STATUS equ 0x81

    org 0x4000
start:
    ld sp, 0x7FFF
    ld hl, msg
tx_lp:
    ld a, (hl)
    or a
    jr z, done
    push hl
tx_w:
    in a, (UART_STATUS)
    and 0x02
    jr z, tx_w
    pop hl
    ld a, (hl)
    out (UART_DATA), a
    inc hl
    jr tx_lp
done:
    halt
    jr done

msg: defm "INIT OK\r\n\0"
