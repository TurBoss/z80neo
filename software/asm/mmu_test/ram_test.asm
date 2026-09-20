; ram_test.asm — Z80NEO RAM write/read verification
;
; Purpose: continuously write patterns to RAM and read them back so the
; firmware's memory WRITE path is exercised. The OLED "W" counter (dw_op)
; and "WDAT" should visibly increment while this runs.
;
; Layout:
;   Code runs from 0x0000 (loaded there by the firmware HEX loader)
;   Stack at 0x3FFF
;   Test buffer at 0x8000..0x80FF (256 bytes, virtual page 2)
;
; Each pass:
;   1. Write an incrementing pattern (seed + index) to the buffer
;   2. Read it back and compare
;   3. Print PASS/FAIL + pass counter via UART
;   4. Bump the seed and repeat forever
;
; If firmware memory writes work, you'll see continuous PASS lines and the
; OLED W counter climbing. A FAIL prints the address + expected/got bytes.

UART_DATA   equ 0x80
UART_STATUS equ 0x81

BUF_ADDR    equ 0x8000
BUF_LEN     equ 0x0100        ; 256 bytes

    org 0x0000
    jp start

; =============================================================================
; Strings
; =============================================================================
banner:
    defm "\r\n=== Z80NEO RAM Write/Read Test ===\r\n"
    defm "Buffer 0x8000..0x80FF, looping forever.\r\n\r\n"
banner_end:

pass_s:  defm "PASS pass#"
pass_e:
fail_s:  defm "FAIL @0x"
fail_e:
exp_s:   defm " exp="
exp_e:
got_s:   defm " got="
got_e:

; =============================================================================
; Entry
; =============================================================================
start:
    ld sp, 0x3FFF

    ld hl, banner
    ld bc, banner_end - banner
    call uart_send_buf

    ld a, 0x00          ; initial seed
    ld (seed), a

    ld hl, 0
    ld (pass_cnt), hl

main_loop:
    ;--------------------------------------------------------------
    ; WRITE phase: buffer[i] = seed + i
    ;--------------------------------------------------------------
    ld hl, BUF_ADDR
    ld b, 0             ; index (0..255), 256 iterations via djnz wrap
    ld a, (seed)
    ld c, a             ; c = seed + index accumulator
write_loop:
    ld a, c
    ld (hl), a          ; RAM WRITE  ← this drives dw_op / WDAT
    inc hl
    inc c
    inc b
    jr nz, write_loop   ; loop 256 times (b wraps 0→...→0)

    ;--------------------------------------------------------------
    ; READ + VERIFY phase
    ;--------------------------------------------------------------
    ld hl, BUF_ADDR
    ld b, 0
    ld a, (seed)
    ld c, a
verify_loop:
    ld a, (hl)          ; RAM READ
    cp c                ; expected == seed + index?
    jr nz, verify_fail
    inc hl
    inc c
    inc b
    jr nz, verify_loop

    ;--------------------------------------------------------------
    ; PASS — print "PASS pass#NNNN"
    ;--------------------------------------------------------------
    ld hl, pass_s
    ld bc, pass_e - pass_s
    call uart_send_buf

    ld hl, (pass_cnt)
    call uart_tx_hex16
    call uart_crlf

    ; Increment pass counter
    ld hl, (pass_cnt)
    inc hl
    ld (pass_cnt), hl

    ; Bump seed for next pass so the pattern changes each round
    ld a, (seed)
    inc a
    ld (seed), a

    jr main_loop

    ;--------------------------------------------------------------
    ; FAIL — print "FAIL @0xAAAA exp=EE got=GG"
    ; HL points at failing byte, C = expected, (HL) = got
    ;--------------------------------------------------------------
verify_fail:
    push hl             ; save fail address
    push bc             ; save expected (c)

    ld hl, fail_s
    ld bc, fail_e - fail_s
    call uart_send_buf

    pop bc              ; c = expected
    pop hl              ; hl = fail address
    push hl
    push bc

    call uart_tx_hex16  ; print address

    ; " exp="
    ld hl, exp_s
    ld bc, exp_e - exp_s
    call uart_send_buf
    pop bc
    push bc
    ld a, c
    call uart_tx_hex    ; expected

    ; " got="
    ld hl, got_s
    ld bc, got_e - got_s
    call uart_send_buf
    pop bc              ; discard
    pop hl              ; hl = fail address
    ld a, (hl)
    call uart_tx_hex    ; got

    call uart_crlf

    ; Halt on failure so the bad value stays on screen
    halt
    jp $

; =============================================================================
; Storage
; =============================================================================
seed:      defs 1
pass_cnt:  defs 2

; =============================================================================
; uart_send_buf — send BC bytes from HL
; =============================================================================
uart_send_buf:
    ld a, b
    or c
    ret z
usb_loop:
    ld a, (hl)
    call uart_tx
    inc hl
    dec bc
    ld a, b
    or c
    jr nz, usb_loop
    ret

; =============================================================================
; uart_tx — transmit byte in A, blocks until TX ready (bit 1)
; =============================================================================
uart_tx:
    push af
ut_wait:
    in a, (UART_STATUS)
    and 0x02
    jr z, ut_wait
    pop af
    out (UART_DATA), a
    ret

; =============================================================================
; uart_tx_hex — print A as 2 hex chars
; =============================================================================
uart_tx_hex:
    push af
    rrca
    rrca
    rrca
    rrca
    call uth_nib
    pop af
    call uth_nib
    ret
uth_nib:
    and 0x0F
    cp 0x0A
    jr c, uth_d
    add 'A' - 0x0A
    jr uart_tx
uth_d:
    add '0'
    jr uart_tx

; =============================================================================
; uart_tx_hex16 — print HL as 4 hex chars
; =============================================================================
uart_tx_hex16:
    ld a, h
    call uart_tx_hex
    ld a, l
    call uart_tx_hex
    ret

; =============================================================================
; uart_crlf — send \r\n
; =============================================================================
uart_crlf:
    push af
    ld a, 0x0D
    call uart_tx
    ld a, 0x0A
    call uart_tx
    pop af
    ret
