;
; Z80 serial bootloader - receives bytes and jumps to them
;

    org 0x0000

    di
    ld sp, 0x2000
    ld hl, 0x0200       ; Start of user code area

receive_loop:
    in a, (0x80)        ; Wait for and read byte
    ld (hl), a          ; Store in memory
    inc hl              ; Increment pointer
    jr receive_loop     ; Continue forever

    ; After loading, you would need a way to jump to the code
    ; This simple version never exits the loop
