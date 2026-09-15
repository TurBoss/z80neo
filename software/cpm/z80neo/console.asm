;****************************************************************************
; console.asm — Console I/O routines for z80neo CP/M BIOS
;****************************************************************************

    INCLUDE "z80neo.inc"

;──────────────────────────────────────────────────────────────────────────
; con_tx_ready — returns NZ if transmitter is ready
;   destroys: AF
;──────────────────────────────────────────────────────────────────────────
con_tx_ready:
    IN      A, (SERIAL_STATUS)
    AND     TX_READY_BIT
    RET                         ; Z = not ready, NZ = ready

;──────────────────────────────────────────────────────────────────────────
; con_rx_ready — returns NZ (A=1) if receiver has data, Z (A=0) if empty
;   destroys: AF
;──────────────────────────────────────────────────────────────────────────
con_rx_ready:
    IN      A, (SERIAL_STATUS)
    AND     RX_READY_BIT
    RET     Z                   ; Z = no data
    LD      A, 1
    OR      A                   ; clear Z, A=1
    RET

;──────────────────────────────────────────────────────────────────────────
; con_tx_char — transmit character in C (waits for TX ready)
;   destroys: AF
;──────────────────────────────────────────────────────────────────────────
con_tx_char:
    CALL    con_tx_ready
    JR      Z, con_tx_char      ; spin until ready
    LD      A, C
    OUT     (SERIAL_PORT), A
    RET

;──────────────────────────────────────────────────────────────────────────
; con_rx_char — wait for and return received character in A
;   destroys: AF
;──────────────────────────────────────────────────────────────────────────
con_rx_char:
    CALL    con_rx_ready
    JR      Z, con_rx_char      ; spin until data available
    IN      A, (SERIAL_PORT)
    AND     7FH                 ; strip parity (bit 7)
    OR      A                   ; check for NUL (0x00)
    JR      Z, con_rx_char      ; discard — firmware race, retry
    RET

;──────────────────────────────────────────────────────────────────────────
; con_rx_char_nb — non-blocking: return char in A with Z clear, or Z set
;   destroys: AF
;──────────────────────────────────────────────────────────────────────────
con_rx_char_nb:
    CALL    con_rx_ready
    RET     Z                   ; no char available
    IN      A, (SERIAL_PORT)
    AND     7FH
    OR      A                   ; clear Z
    RET
