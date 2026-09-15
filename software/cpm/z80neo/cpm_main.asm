;****************************************************************************
; cpm_main.asm — CP/M 2.2 CCP + BDOS + BIOS for z80neo
;
; Assembled at CPM_BASE (0xD400). The binary includes zero-padding from
; address 0 to CPM_BASE, resulting in a 60KB+ binary. srec_cat maps it
; correctly and excludes the boot page to avoid overlap with boot.bin.
;
; Labels resolve to correct runtime addresses (0xDxxx range).
;****************************************************************************

    INCLUDE "z80neo.inc"

    ORG     CPM_BASE

    ; ── CCP + BDOS ─────────────────────────────────────────────────────
    INCLUDE "cpm22.asm"

    ; ── Pad to BIOS jump table ─────────────────────────────────────────
    DEFS    BIOS_BASE - $

    ; ── BIOS (jump table + all 17 routines + disk + console) ────────────
    INCLUDE "bios.asm"
