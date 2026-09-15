;****************************************************************************
; boot.asm — CP/M 2.2 page zero boot vectors for z80neo
;
; Cold-start vector at 0x0000 jumps directly to BIOS BOOT routine.
; The BIOS is at CPM_BASE+0x1600 (0xEA00) which contains the jump table.
; BOOT is the first entry in the table, so we jump to 0xEA00.
;
; The hex loader replicates this to all MMU banks.
;****************************************************************************

    INCLUDE "z80neo.inc"

    ORG     0000H

    ; Cold start — JP to BIOS BOOT (first entry in BIOS jump table at BIOS_BASE)
    JP      BIOS_BASE

    ; Pad to 0x0005 (BDOS entry initialized by BIOS at runtime)
    DEFS    0005H - $

    ; Fill remaining page zero with zeros
    DEFS    0100H - $
