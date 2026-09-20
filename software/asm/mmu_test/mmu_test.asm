; mmu_test.asm — Z80NEO MMU A0 stuck-high diagnostic
;
; Firmware MMU register mapping (from firmware/memory.c):
;   in/out 0xF1 ↔ mmu_page[0]
;   in/out 0xF3 ↔ mmu_page[1]
;   in/out 0xF5 ↔ mmu_page[2]
;   in/out 0xF7 ↔ mmu_page[3]
;   ANY other port → returns 0x00 / ignored on write
;
; Hypothesis: A0 line is stuck HIGH on the address bus mux.
;   Z80 puts 0xF0 on A0-A7 → Pico reads 0xF1 (A0 forced to 1)
;   Z80 puts 0xF1 on A0-A7 → Pico reads 0xF1
;
; Test per pair (even_port, odd_port):
;   1. Read original value from ODD port (the canonical firmware port)
;   2. Write a NEW value (≠ original) to the EVEN port
;   3. Read from the ODD port
;      → If result == NEW value: EVEN port write reached ODD port's register
;        This means A0 is stuck high (Z80's 0xF0 became Pico's 0xF1)
;   4. Restore original value
;
; Cross-pair test: write 4 unique values to 0xF0,F2,F4,F6,
; read back via 0xF1,F3,F5,F7 → must be 4 distinct values.

UART_DATA   equ 0x80
UART_STATUS equ 0x81

    org 0x0000
    jp start

; =============================================================================
; Strings
; =============================================================================
banner:
    defm "\r\n=== Z80NEO MMU A0 Stuck-High Test ===\r\n\r\n"
banner_end:

pass_s:  defm "  PASS — ports are distinct\r\n"
pass_e:
fail_s:  defm "  FAIL — A0 stuck high, collision detected\r\n"
fail_e:

pair01_s:  defm "0xF0 vs 0xF1:"
pair01_e:
pair23_s:  defm "0xF2 vs 0xF3:"
pair23_e:
pair45_s:  defm "0xF4 vs 0xF5:"
pair45_e:
pair67_s:  defm "0xF6 vs 0xF7:"
pair67_e:

sep_s:
    defm "\r\n--- Cross-pair isolation (4 distinct registers?) ---\r\n"
sep_e:

cross_s:  defm "0xF0,F2,F4,F6:"
cross_e:

done_s:
    defm "\r\nTest complete. System halted.\r\n"
done_e:

; Save area for original MMU values (read via odd ports before test)
orig0:  defs 1
orig1:  defs 1
orig2:  defs 1
orig3:  defs 1

; =============================================================================
; Entry point
; =============================================================================
start:
    ld sp, 0x3FFF

    ; Print banner
    ld hl, banner
    ld bc, banner_end - banner
    call uart_send_buf

    ;--- Save all original MMU values (read via odd ports) ---
    in a, (0xF1)
    ld (orig0), a
    in a, (0xF3)
    ld (orig1), a
    in a, (0xF5)
    ld (orig2), a
    in a, (0xF7)
    ld (orig3), a

    ;--- Test pair 0xF0/0xF1 ---
    ld hl, pair01_s
    ld bc, pair01_e - pair01_s
    call uart_send_buf
    ld a, (orig0)
    ld b, a               ; B = original value
    call test_pair_01
    jr nz, p01_fail
    ld hl, pass_s
    ld bc, pass_e - pass_s
    call uart_send_buf
    jr p01_done
p01_fail:
    ld hl, fail_s
    ld bc, fail_e - fail_s
    call uart_send_buf
    ; restore original
    ld a, (orig0)
    out (0xF1), a
p01_done:

    ;--- Test pair 0xF2/0xF3 ---
    ld hl, pair23_s
    ld bc, pair23_e - pair23_s
    call uart_send_buf
    ld a, (orig1)
    ld b, a
    call test_pair_23
    jr nz, p23_fail
    ld hl, pass_s
    ld bc, pass_e - pass_s
    call uart_send_buf
    jr p23_done
p23_fail:
    ld hl, fail_s
    ld bc, fail_e - fail_s
    call uart_send_buf
    ld a, (orig1)
    out (0xF3), a
p23_done:

    ;--- Test pair 0xF4/0xF5 ---
    ld hl, pair45_s
    ld bc, pair45_e - pair45_s
    call uart_send_buf
    ld a, (orig2)
    ld b, a
    call test_pair_45
    jr nz, p45_fail
    ld hl, pass_s
    ld bc, pass_e - pass_s
    call uart_send_buf
    jr p45_done
p45_fail:
    ld hl, fail_s
    ld bc, fail_e - fail_s
    call uart_send_buf
    ld a, (orig2)
    out (0xF5), a
p45_done:

    ;--- Test pair 0xF6/0xF7 ---
    ld hl, pair67_s
    ld bc, pair67_e - pair67_s
    call uart_send_buf
    ld a, (orig3)
    ld b, a
    call test_pair_67
    jr nz, p67_fail
    ld hl, pass_s
    ld bc, pass_e - pass_s
    call uart_send_buf
    jr p67_done
p67_fail:
    ld hl, fail_s
    ld bc, fail_e - fail_s
    call uart_send_buf
    ld a, (orig3)
    out (0xF7), a
p67_done:

    ;--- Cross-pair isolation ---
    ld hl, sep_s
    ld bc, sep_e - sep_s
    call uart_send_buf

    ld hl, cross_s
    ld bc, cross_e - cross_s
    call uart_send_buf

    ; Write unique values to each even port
    ld a, 0x11
    out (0xF0), a
    ld a, 0x22
    out (0xF2), a
    ld a, 0x33
    out (0xF4), a
    ld a, 0x44
    out (0xF6), a

    ; Read back via odd ports
    in a, (0xF1)
    cp 0x11
    jr nz, cross_fail
    in a, (0xF3)
    cp 0x22
    jr nz, cross_fail
    in a, (0xF5)
    cp 0x33
    jr nz, cross_fail
    in a, (0xF7)
    cp 0x44
    jr nz, cross_fail

    ld hl, pass_s
    ld bc, pass_e - pass_s
    call uart_send_buf
    jr restore_all

cross_fail:
    ld hl, fail_s
    ld bc, fail_e - fail_s
    call uart_send_buf

    ;--- Restore all original MMU values ---
restore_all:
    ld a, (orig0)
    out (0xF1), a
    ld a, (orig1)
    out (0xF3), a
    ld a, (orig2)
    out (0xF5), a
    ld a, (orig3)
    out (0xF7), a

    ; Done
    ld hl, done_s
    ld bc, done_e - done_s
    call uart_send_buf

    halt
    jp $           ; should never reach here

; =============================================================================
; Pair tests — write to EVEN port, read from ODD port
;
; Input:  B = original value of the odd (canonical) port
; Output: Z=1 if A0 is stuck (even port aliased to odd)
;         Z=0 if ports are distinct (no collision)
;
; For each pair, we pick a test value that differs from B (the original).
; If A0 is stuck: writing to the even port reaches the odd port's MMU register.
;   → reading the odd port returns TEST_VAL, not B.
; =============================================================================

test_pair_01:
    ; Pick a test value different from original
    ld a, b
    cp 0x55
    jr z, tp01_alt
    ld c, 0x55
    jr tp01_go
tp01_alt:
    ld c, 0xAA
tp01_go:
    ; Write test value to EVEN port 0xF0
    ld a, c
    out (0xF0), a
    ; Read from ODD port 0xF1
    in a, (0xF1)
    cp c
    jr z, tp01_stuck     ; even write changed odd → A0 stuck
    ; No collision — restore original (odd port unchanged by even write)
    ld a, b
    out (0xF1), a
    xor a                ; Z=0 (no collision)
    ld a, 1
    or a
    ret
tp01_stuck:
    ; Collision! Z=1
    xor a
    ret

test_pair_23:
    ld a, b
    cp 0x55
    jr z, tp23_alt
    ld c, 0x55
    jr tp23_go
tp23_alt:
    ld c, 0xAA
tp23_go:
    ld a, c
    out (0xF2), a
    in a, (0xF3)
    cp c
    jr z, tp23_stuck
    ld a, b
    out (0xF3), a
    xor a
    ld a, 1
    or a
    ret
tp23_stuck:
    xor a
    ret

test_pair_45:
    ld a, b
    cp 0x55
    jr z, tp45_alt
    ld c, 0x55
    jr tp45_go
tp45_alt:
    ld c, 0xAA
tp45_go:
    ld a, c
    out (0xF4), a
    in a, (0xF5)
    cp c
    jr z, tp45_stuck
    ld a, b
    out (0xF5), a
    xor a
    ld a, 1
    or a
    ret
tp45_stuck:
    xor a
    ret

test_pair_67:
    ld a, b
    cp 0x55
    jr z, tp67_alt
    ld c, 0x55
    jr tp67_go
tp67_alt:
    ld c, 0xAA
tp67_go:
    ld a, c
    out (0xF6), a
    in a, (0xF7)
    cp c
    jr z, tp67_stuck
    ld a, b
    out (0xF7), a
    xor a
    ld a, 1
    or a
    ret
tp67_stuck:
    xor a
    ret

; =============================================================================
; uart_send_buf — send BC bytes from HL via UART
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
; uart_tx — transmit byte in A, blocks until TX ready (bit 1 of status)
; =============================================================================
uart_tx:
    push af
ut_wait:
    in a, (UART_STATUS)
    and 0x02            ; bit 1 = TX buffer empty / ready
    jr z, ut_wait
    pop af
    out (UART_DATA), a
    ret
