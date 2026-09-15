; bios.asm — CP/M 2.2 BIOS for z80neo
;
; Structure:
;   1. BIOS jump table (at BIOS_BASE)
;   2. BIOS wrapper routines
;   3. Console and disk implementations (included at end)
;   4. Private stack
;****************************************************************************

    INCLUDE "z80neo.inc"

;==========================================================================
; BIOS jump table — must start exactly at BIOS_OFFSET (0x1700) from CPM_BASE
;==========================================================================

;==========================================================================
; B I O S   J U M P   T A B L E
;
; 17 entries × 3 bytes = 51 bytes total.
; The BDOS calls these via absolute addresses.
;==========================================================================

BOOT:   JP      bios_boot
WBOOT:  JP      bios_wboot
CONST:  JP      bios_const
CONIN:  JP      bios_conin
CONOUT: JP      bios_conout
LIST:   JP      bios_list
PUNCH:  JP      bios_punch
READER: JP      bios_reader
HOME:   JP      disk_home
SELDSK: JP      disk_seldsk
SETTRK: JP      disk_settrk
SETSEC: JP      disk_setsec
SETDMA: JP      disk_setdma
READ:   JP      disk_read
WRITE:  JP      disk_write
PRSTAT: JP      bios_prstat
SECTRN: JP      disk_sectrn

;==========================================================================
; B I O S   W R A P P E R   R O U T I N E S
;
; These thin wrappers match the CP/M 2.2 BIOS calling convention
; and delegate to the actual console/disk implementations below.
;==========================================================================

;──────────────────────────────────────────────────────────────────────────
; Cold start — initialize system and jump to CCP
;──────────────────────────────────────────────────────────────────────────
bios_boot:
bios_boot_cont:
    LD      SP, bios_stack
    LD      HL, signon_msg
    CALL    puts

    ; Drain any stale characters from UART RX buffer before starting CP/M.
    ; Without this, spurious boot-time bytes (noise, terminal chatter) get
    ; interpreted as Ctrl-C by BDOS CKCONSOL/RDBUFF and cause a warm boot loop.
bios_drain:
    CALL    con_rx_ready
    JR      Z, bios_drained
    CALL    con_rx_char             ; read and discard one char
    JR      bios_drain
bios_drained:

    ; Set up page zero vectors
    LD      A, 0C3H
    LD      (0000H), A
    LD      HL, WBOOT
    LD      (0001H), HL             ; 0x0000 = JP WBOOT

    LD      A, 0C3H
    LD      (0005H), A
	    LD      HL, CPM_BASE+080CH      ; BDOS entry (JP at 0xDC0C -> dispatcher)
    LD      (0006H), HL             ; 0x0005 = JP FBASE

    XOR     A
    LD      (0003H), A              ; IOBYTE = 0
    LD      (0004H), A              ; current drive = A

    ; Clear command buffer at 0x80 so CCP doesn't execute garbage
    XOR     A
    LD      (0080H), A              ; maxlen=0 → no command waiting
    LD      (0081H), A              ; actuallen=0

    LD      BC, 0080H
    CALL    disk_setdma

    LD      C, 0                    ; select drive A
    JP      CPM_BASE

;──────────────────────────────────────────────────────────────────────────
; Warm start — reload CCP+BDOS from disk and restart
;──────────────────────────────────────────────────────────────────────────
bios_wboot:
    LD      SP, bios_stack

    ; Skip disk reload — CCP+BDOS already in RAM (loaded from hex)
    ; Just reinitialize page zero and restart
    JP      bios_boot_cont

;──────────────────────────────────────────────────────────────────────────
; Console wrappers
;──────────────────────────────────────────────────────────────────────────
bios_const:
    CALL    con_rx_ready
    RET     Z
    LD      A, 0FFH
    RET

bios_conin:
    JP      con_rx_char

bios_conout:
    JP      con_tx_char

bios_list:
    JP      con_tx_char

bios_punch:
    RET

bios_reader:
    LD      A, 1AH                  ; Ctrl-Z = EOF
    RET

bios_prstat:
    LD      A, 0FFH
    RET

;──────────────────────────────────────────────────────────────────────────
; puts — print null-terminated string at HL to console
;──────────────────────────────────────────────────────────────────────────
puts:
    LD      A, (HL)
    OR      A
    RET     Z
    PUSH    HL
    LD      C, A
    CALL    con_tx_char
    POP     HL
    INC     HL
    JR      puts

;──────────────────────────────────────────────────────────────────────────
; Boot messages
;──────────────────────────────────────────────────────────────────────────
signon_msg:
    DEFB    0DH, 0AH
    DEFB    "z80neo CP/M 2.2 BIOS v1.0", 0DH, 0AH
    DEFB    "64K RAM, Disk A: SD image", 0DH, 0AH
    DEFB    0DH, 0AH, 0
