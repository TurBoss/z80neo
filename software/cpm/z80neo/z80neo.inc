;****************************************************************************
; z80neo.inc — Hardware definitions for z80neo CP/M BIOS
;
; Port addresses and status bit masks for the z80neo system.
;****************************************************************************
    IFNDEF Z80NEO_INC
    DEFINE Z80NEO_INC

; ── Serial (UART via Pico USB-UART bridge) ─────────────────────────────

SERIAL_PORT:    EQU     80H             ; serial data port
SERIAL_STATUS:  EQU     81H             ; serial status port
TX_READY_BIT:   EQU     02H             ; bit 1 = TX buffer has room
RX_READY_BIT:   EQU     01H             ; bit 0 = RX data available

; ── Disk I/O (CP/M disk image on SD, mediated by Pico) ─────────────────

DISK_CMD_DATA:  EQU     0E0H            ; R/W: sector data or command byte
DISK_TRACK:     EQU     0E1H            ; W: track number (0–255)
DISK_SECTOR:    EQU     0E2H            ; W: sector number / R: status (0=OK)
DISK_STATUS:    EQU     0E2H            ; R: status (read via sector port)
DISK_SELECT:    EQU     0E3H            ; W: drive select (0=A, 1=B, ...)
DISK_DRIVES:    EQU     15              ; number of emulated drives (A-O)

DISK_CMD_READ:  EQU     00H             ; read 128-byte sector into Pico buffer
DISK_CMD_WRITE: EQU     01H             ; collect 128 bytes, then write to SD

; ── CP/M geometry defaults ─────────────────────────────────────────────

SECS_PER_TRACK: EQU     26              ; 8" SSSD standard
NUM_TRACKS:     EQU     254             ; max usable with 8-bit track reg
SEC_SIZE:       EQU     128

; ── Memory layout (60K CP/M system) ────────────────────────────────────

MEM:            EQU     60
CPM_BASE:       EQU     0D400H          ; fixed: (60-7)*1024
BIOS_BASE:      EQU     0EB00H          ; CPM_BASE+0x1700

    ENDIF ; Z80NEO_INC
