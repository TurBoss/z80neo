; bus_test.asm — Z80NEO comprehensive bus diagnostic
; Tests:
;   1. A0 stuck-high (MMU even/odd port collision)
;   2. D0 stuck (data bus bit 0)
;   3. A8 stuck (address high byte bit 0)
;   4. Cross-pair MMU register isolation

UART_DATA   equ 0x80
UART_STATUS equ 0x81

STRING_CRLF  equ 1
STRING_HEX   equ 2

    org 0x0000
    jp start

; =============================================================================
; Strings
; =============================================================================
banner:
    defm "\r\n=== Z80NEO Bus Diagnostic ===\r\n\r\n"
banner_end:

; --- A0 stuck-high test ---
a0_title:  defm "--- A0 stuck-high (MMU even/odd collision) ---\r\n"
a0_title_e:
a0_pair01: defm "0xF0 vs 0xF1: "
a0_pair01_e:
a0_pair23: defm "0xF2 vs 0xF3: "
a0_pair23_e:
a0_pair45: defm "0xF4 vs 0xF5: "
a0_pair45_e:
a0_pair67: defm "0xF6 vs 0xF7: "
a0_pair67_e:

a0_stuck:   defm "COLLISION - A0 stuck high\r\n"
a0_stuck_e:
a0_ok:      defm "OK - ports distinct\r\n"
a0_ok_e:

; --- D0 stuck test ---
d0_title:  defm "\r\n--- D0 stuck (data bus bit 0) ---\r\n"
d0_title_e:
d0_write:  defm "Write AA/55 to MMU reg, read back: "
d0_write_e:
d0_stuck:  defm "STUCK - bit 0 doesn't flip\r\n"
d0_stuck_e:
d0_ok:     defm "OK - bit 0 toggles\r\n"
d0_ok_e:
d0_wrote_aa: defm "  wrote 0xAA, read "
d0_wrote_aa_e:
d0_wrote_55: defm "  wrote 0x55, read "
d0_wrote_55_e:

; --- A8 stuck test ---
a8_title:  defm "\r\n--- A8 stuck (address high bit 0) ---\r\n"
a8_title_e:
a8_test:   defm "Write to 0x0000 vs 0x0100: "
a8_test_e:
a8_stuck:  defm "STUCK - both addresses return same value\r\n"
a8_stuck_e:
a8_ok:     defm "OK - addresses are distinct\r\n"
a8_ok_e:
a8_0000:   defm "  [0x0000] = "
a8_0000_e:
a8_0100:   defm "  [0x0100] = "
a8_0100_e:

; --- Cross-pair MMU isolation ---
cross_title: defm "\r\n--- MMU cross-pair isolation ---\r\n"
cross_title_e:
cross_test:  defm "4 distinct regs (0xF0,F2,F4,F6): "
cross_test_e:
cross_stuck: defm "FAIL - registers not all distinct\r\n"
cross_stuck_e:
cross_ok:    defm "OK - 4 independent registers\r\n"
cross_ok_e:

done_s:
    defm "\r\n=== All tests complete ===\r\n"
done_e:

; Storage
orig_mmu:  defs 4   ; original MMU values from odd ports
scratch:   defs 64   ; small buffer

; =============================================================================
; Start
; =============================================================================
start:
    ld sp, 0x3FFF

    ; Print banner
    ld hl, banner
    ld bc, banner_end - banner
    call uart_send_buf

    ; Save original MMU values
    in a, (0xF1)
    ld (orig_mmu+0), a
    in a, (0xF3)
    ld (orig_mmu+1), a
    in a, (0xF5)
    ld (orig_mmu+2), a
    in a, (0xF7)
    ld (orig_mmu+3), a

    ;--------------------------------------------------------------
    ; TEST 1: A0 stuck-high
    ;--------------------------------------------------------------
    ld hl, a0_title
    ld bc, a0_title_e - a0_title
    call uart_send_buf

    ; Test each pair: write to even, read from odd
    ld hl, a0_pair01
    ld bc, a0_pair01_e - a0_pair01
    call uart_send_buf
    ld a, (orig_mmu+0)
    ld b, a            ; original value
    ld d, 0xF0         ; even port
    ld e, 0xF1         ; odd port
    call test_a0_collision
    call print_result

    ld hl, a0_pair23
    ld bc, a0_pair23_e - a0_pair23
    call uart_send_buf
    ld a, (orig_mmu+1)
    ld b, a
    ld d, 0xF2
    ld e, 0xF3
    call test_a0_collision
    call print_result

    ld hl, a0_pair45
    ld bc, a0_pair45_e - a0_pair45
    call uart_send_buf
    ld a, (orig_mmu+2)
    ld b, a
    ld d, 0xF4
    ld e, 0xF5
    call test_a0_collision
    call print_result

    ld hl, a0_pair67
    ld bc, a0_pair67_e - a0_pair67
    call uart_send_buf
    ld a, (orig_mmu+3)
    ld b, a
    ld d, 0xF6
    ld e, 0xF7
    call test_a0_collision
    call print_result

    ;--------------------------------------------------------------
    ; TEST 2: D0 stuck (data bus bit 0)
    ;--------------------------------------------------------------
    ld hl, d0_title
    ld bc, d0_title_e - d0_title
    call uart_send_buf

    ld hl, d0_write
    ld bc, d0_write_e - d0_write
    call uart_send_buf

    ; Write 0xAA to mmu_page[0], read back
    ld a, 0xAA
    out (0xF1), a       ; write to odd port directly
    in a, (0xF1)        ; read back
    ld c, a             ; save result

    ld hl, d0_wrote_aa
    ld bc, d0_wrote_aa_e - d0_wrote_aa
    call uart_send_buf
    ld a, c
    call uart_tx_hex
    call uart_crlf

    ; Write 0x55 to mmu_page[0], read back
    ld a, 0x55
    out (0xF1), a
    in a, (0xF1)
    ld c, a

    ld hl, d0_wrote_55
    ld bc, d0_wrote_55_e - d0_wrote_55
    call uart_send_buf
    ld a, c
    call uart_tx_hex
    call uart_crlf

    ; Check: both writes should change bit 0. If 0xAA→0xAA and 0x55→0x55, bit 0 flips.
    ; If both read as same value with bit0 unchanged, D0 stuck.
    ld a, 0xAA
    out (0xF1), a
    in a, (0xF1)
    ld c, a             ; c = readback of 0xAA
    ld a, 0x55
    out (0xF1), a
    in a, (0xF1)        ; a = readback of 0x55
    cp c
    jr z, d0_is_stuck   ; if same → D0 stuck (or other bits stuck)
    ; Values differ → check specifically bit 0
    ld a, c             ; readback of 0xAA
    and 0x01
    ld b, a             ; b = bit0 of AA readback (should be 0)
    ld a, 0x55
    out (0xF1), a
    in a, (0xF1)
    and 0x01            ; bit0 of 55 readback (should be 1)
    cp b
    jr z, d0_is_stuck   ; same bit0 → stuck

    ld hl, d0_ok
    ld bc, d0_ok_e - d0_ok
    call uart_send_buf
    jr d0_done

d0_is_stuck:
    ld hl, d0_stuck
    ld bc, d0_stuck_e - d0_stuck
    call uart_send_buf

d0_done:
    ; Restore mmu_page[0]
    ld a, (orig_mmu+0)
    out (0xF1), a

    ;--------------------------------------------------------------
    ; TEST 3: A8 stuck (address high byte bit 0)
    ; Write to 0x0000 and 0x0100, verify they're distinct.
    ;--------------------------------------------------------------
    ld hl, a8_title
    ld bc, a8_title_e - a8_title
    call uart_send_buf

    ld hl, a8_test
    ld bc, a8_test_e - a8_test
    call uart_send_buf

    ; Save original values at 0x0000 and 0x0100
    ld a, (0x0000)
    ld b, a             ; b = orig[0x0000]
    ld a, (0x0100)
    ld c, a             ; c = orig[0x0100]

    ; Write 0x11 to 0x0000, 0xEE to 0x0100
    ld a, 0x11
    ld (0x0000), a
    ld a, 0xEE
    ld (0x0100), a

    ; Read back both
    ld a, (0x0000)
    ld d, a             ; d = readback 0x0000
    ld a, (0x0100)
    ld e, a             ; e = readback 0x0100

    ; Print results
    ld hl, a8_0000
    ld bc, a8_0000_e - a8_0000
    call uart_send_buf
    ld a, d
    call uart_tx_hex
    call uart_crlf

    ld hl, a8_0100
    ld bc, a8_0100_e - a8_0100
    call uart_send_buf
    ld a, e
    call uart_tx_hex
    call uart_crlf

    ; Restore originals
    ld a, b
    ld (0x0000), a
    ld a, c
    ld (0x0100), a

    ; Check: if d != e, A8 works
    ld a, d
    cp e
    jr z, a8_is_stuck

    ld hl, a8_ok
    ld bc, a8_ok_e - a8_ok
    call uart_send_buf
    jr a8_done

a8_is_stuck:
    ld hl, a8_stuck
    ld bc, a8_stuck_e - a8_stuck
    call uart_send_buf

a8_done:

    ;--------------------------------------------------------------
    ; TEST 4: Cross-pair MMU isolation
    ;--------------------------------------------------------------
    ld hl, cross_title
    ld bc, cross_title_e - cross_title
    call uart_send_buf

    ld hl, cross_test
    ld bc, cross_test_e - cross_test
    call uart_send_buf

    ; Write 4 unique values to 4 even ports
    ld a, 0xA1
    out (0xF0), a
    ld a, 0xB2
    out (0xF2), a
    ld a, 0xC3
    out (0xF4), a
    ld a, 0xD4
    out (0xF6), a

    ; Read back via odd ports
    in a, (0xF1)
    cp 0xA1
    jr nz, cross_is_stuck
    in a, (0xF3)
    cp 0xB2
    jr nz, cross_is_stuck
    in a, (0xF5)
    cp 0xC3
    jr nz, cross_is_stuck
    in a, (0xF7)
    cp 0xD4
    jr nz, cross_is_stuck

    ld hl, cross_ok
    ld bc, cross_ok_e - cross_ok
    call uart_send_buf
    jr cross_done

cross_is_stuck:
    ld hl, cross_stuck
    ld bc, cross_stuck_e - cross_stuck
    call uart_send_buf

cross_done:

    ;--------------------------------------------------------------
    ; Restore all MMU values
    ;--------------------------------------------------------------
    ld a, (orig_mmu+0)
    out (0xF1), a
    ld a, (orig_mmu+1)
    out (0xF3), a
    ld a, (orig_mmu+2)
    out (0xF5), a
    ld a, (orig_mmu+3)
    out (0xF7), a

    ; Done
    ld hl, done_s
    ld bc, done_e - done_s
    call uart_send_buf

    halt
    jp $

; =============================================================================
; test_a0_collision — test if even_port(D) and odd_port(E) share same register
;
; Input:  B = original value of odd port
;         D = even port address
;         E = odd port address
; Output: Z=1 if collision (A0 stuck), Z=0 if distinct
; =============================================================================
test_a0_collision:
    ; Pick a test value different from original
    ld a, b
    cp 0x55
    jr z, tac_alt
    ld c, 0x55
    jr tac_go
tac_alt:
    ld c, 0xAA
tac_go:
    ; Save C (test value) and set up port in C for 'out (c),a'
    push bc
    ; Write test value to EVEN port
    ld a, c             ; a = test value
    ld c, d             ; c = even port address
    out (c), a          ; out via even port
    ; Read from ODD port (Z80 only allows 'in a,(c)')
    ld c, e             ; c = odd port address
    in a, (c)           ; read from odd port
    pop bc              ; restore c = test value
    cp c                ; if even write reached odd port's register → A0 stuck
    ret                 ; Z=1 collision, Z=0 distinct
    ; Note: caller must restore original MMU value!

; =============================================================================
; print_result — print "COLLISION" or "OK" based on Z flag
; Input: Z=1 → collision, Z=0 → OK
; =============================================================================
print_result:
    jr z, pr_stuck
    ld hl, a0_ok
    ld bc, a0_ok_e - a0_ok
    call uart_send_buf
    ret
pr_stuck:
    ld hl, a0_stuck
    ld bc, a0_stuck_e - a0_stuck
    call uart_send_buf
    ret

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
; uart_tx — transmit byte, blocks until TX ready (bit 1)
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
