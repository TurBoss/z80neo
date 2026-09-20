; flashtest.asm — flash disk test with commit

FLASH_DATA equ 0xE0
FLASH_ADL  equ 0xE1
FLASH_ADM  equ 0xE2
FLASH_ADH  equ 0xE3
UART_DATA  equ 0x80
UART_STAT  equ 0x81

    org 0x0000

    ld hl, msg_write
    call print
    xor a
    out (FLASH_ADL), a
    out (FLASH_ADM), a
    out (FLASH_ADH), a
    ld hl, test_data
    ld b, test_end - test_data
wr: ld a, (hl)
    out (FLASH_DATA), a
    inc hl
    djnz wr
    ld a, 0x04        ; COMMIT
    out (FLASH_DATA), a

    ld hl, msg_commit
    call print

    xor a
    out (FLASH_ADL), a
    out (FLASH_ADM), a
    out (FLASH_ADH), a
    ld hl, msg_read
    call print
    ld b, test_end - test_data
rd: in a, (FLASH_DATA)
    call tx
    djnz rd
    ld hl, msg_ok
    call print
    halt
    jr $

print: ld a, (hl)
    or a
    ret z
    call tx
    inc hl
    jr print
tx: push af
tx1: in a, (UART_STAT)
    and 0x02
    jr z, tx1
    pop af
    out (UART_DATA), a
    ret

msg_write:  defm "Write+Commit...", 13, 10, 0
msg_commit: defm "Committed.", 13, 10, 0
msg_read:   defm "Read: ", 0
msg_ok:     defm 13, 10, "PERSISTENT OK!", 13, 10, 0
test_data:  defm "FLASH PERSISTENCE!"
test_end:
