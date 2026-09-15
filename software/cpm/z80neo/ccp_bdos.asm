; ccp_bdos.asm — CP/M 2.2 CCP + BDOS only
; Assembled at CPM_BASE. srec_cat maps to correct address.
    INCLUDE "z80neo.inc"

; BDOS calls BIOS via jump table at BIOS_BASE. Define the absolute
; addresses for each entry (each is JP xxxx = 3 bytes).
BOOT:   EQU     BIOS_BASE + 0*3
WBOOT:  EQU     BIOS_BASE + 1*3
CONST:  EQU     BIOS_BASE + 2*3
CONIN:  EQU     BIOS_BASE + 3*3
CONOUT: EQU     BIOS_BASE + 4*3
LIST:   EQU     BIOS_BASE + 5*3
PUNCH:  EQU     BIOS_BASE + 6*3
READER: EQU     BIOS_BASE + 7*3
HOME:   EQU     BIOS_BASE + 8*3
SELDSK: EQU     BIOS_BASE + 9*3
SETTRK: EQU     BIOS_BASE + 10*3
SETSEC: EQU     BIOS_BASE + 11*3
SETDMA: EQU     BIOS_BASE + 12*3
READ:   EQU     BIOS_BASE + 13*3
WRITE:  EQU     BIOS_BASE + 14*3
PRSTAT: EQU     BIOS_BASE + 15*3
SECTRN: EQU     BIOS_BASE + 16*3

    ORG     CPM_BASE
    INCLUDE "cpm22.asm"
