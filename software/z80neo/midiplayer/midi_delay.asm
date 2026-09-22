;
; midi_delay.asm - calibrated busy-wait for the z80neo MIDI player
;
;   void midi_delay_ms(uint16_t ms);
;
; Waits approximately `ms` milliseconds.  The board's Z80 runs at 128 kHz
; (firmware CPU_SPEED), but every memory access also pays the Pico's bus
; service (the 3/3/3 us settles plus the PIO round trip, ~9.6 us measured), so
; a loop calibrated on T-states alone runs ~25% long.  `ld de,2` accounts for
; that.  One outer iteration is:
;
;     push bc            11 T   (1 access)
;     ld de,2            10 T   (2 accesses)
;   inner: dec de                6 T   (1)
;          ld a,d               4 T   (1)
;          or e                 4 T   (1)
;          jr nz,inner         12 T taken / 7 T last   (2)
;     pop bc             10 T   (1)
;     dec bc              6 T   (1)
;     ld a,b               4 T   (1)
;     or c                 4 T   (1)
;     jr nz,outer         12 T   (2 accesses)
;
;   T-states = 26*2 - 5 + 57 = 104 -> 104/128000 = 0.813 ms
;   accesses = 5*2 + 10 = 20      -> 20 * 9.6 us = 0.192 ms
;   total ~= 1.005 ms per outer iteration.
;
; Measured on the board: midi_delay_ms(2000) took 2.51 s with the old de=3.
; Retune if CPU_SPEED or the bus settle times in z80bus_pio.c change.
;

    SECTION code_user

    PUBLIC  _midi_delay_ms
    PUBLIC  _midi_play

; ---------------------------------------------------------------------------
; void midi_play(void);
;
; Streams grouped records from the firmware MIDI API (port 0xA2) and plays
; them.  Each record is [delay lo, delay hi, byte count] followed by
; `byte count` raw MIDI bytes (0 = delay only, 0xFF = end of song).  Waits the
; delay once, then blits the whole tick's bytes from a tight copy loop, so the
; per-record delay/console overhead is paid once per tick instead of once per
; event.  Returns on the 0xFF end sentinel or when 'q'/ESC is typed.
; ---------------------------------------------------------------------------
_midi_play:
mlp_loop:
    in      a, ($A2)        ; delay lo
    ld      c, a
    in      a, ($A2)        ; delay hi
    ld      b, a
    in      a, ($A2)        ; byte count (0xFF = end of song)
    ld      e, a
    cp      $FF
    ret     z               ; end of song

    ld      a, b
    or      c
    jr      z, mlp_send     ; no wait
    push    de
    push    bc              ; wait ms (also the call argument)
    call    _midi_delay_ms
    pop     bc
    pop     de

mlp_send:
    ld      a, e
    or      a
    jr      z, mlp_check    ; delay-only record: nothing to emit

mlp_tx:
    in      a, ($91)        ; one TX-ready check per record
    and     $02
    jr      z, mlp_tx

mlp_byte:
    in      a, ($A2)        ; raw MIDI byte
    out     ($90), a
    dec     e
    jr      nz, mlp_byte

mlp_check:
    in      a, ($81)
    and     $01
    jr      z, mlp_loop
    in      a, ($80)
    cp      'q'
    jr      z, mlp_stop
    cp      'Q'
    jr      z, mlp_stop
    cp      27
    jr      z, mlp_stop
    jr      mlp_loop

; ---------------------------------------------------------------------------
; Manual stop: send a MIDI panic so the synth does not leave notes hanging.
; All Sound Off (CC120), All Notes Off (CC123) and Reset All Controllers
; (CC121), value 0, on every one of the 16 channels.
; ---------------------------------------------------------------------------
mlp_stop:
    call    mlp_panic
    ret

mlp_panic:
    ld      c, $B0                  ; control change, channel 1
mlp_pan_next:
    ld      b, 3                    ; three controllers
    ld      hl, mlp_pan_ctrl
mlp_pan_cc:
    ld      a, c
    call    mlp_tx_byte
    ld      a, (hl)
    call    mlp_tx_byte
    xor     a
    call    mlp_tx_byte
    inc     hl
    djnz    mlp_pan_cc
    inc     c
    ld      a, c
    cp      $C0                     ; past channel 16?
    jr      nz, mlp_pan_next
    ret

mlp_pan_ctrl:
    db      120, 123, 121

; Send one byte, waiting for UART1 TX-ready first (per-byte flow control).
; Preserves A (the data byte), B, C and HL.
mlp_tx_byte:
    push    af
mlp_tx_wait:
    in      a, ($91)
    and     $02
    jr      z, mlp_tx_wait
    pop     af
    out     ($90), a
    ret

_midi_delay_ms:
    ld      hl, 2
    add     hl, sp
    ld      c, (hl)
    inc     hl
    ld      b, (hl)         ; bc = ms
    ld      a, b
    or      c
    ret     z

outer:
    push    bc
    ld      de, 2
inner:
    dec     de
    ld      a, d
    or      e
    jr      nz, inner
    pop     bc
    dec     bc
    ld      a, b
    or      c
    jr      nz, outer
    ret
