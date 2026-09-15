; bios_only.asm — CP/M 2.2 BIOS (jump table + routines)
; Assembled at BIOS_BASE. srec_cat maps to correct address.
    INCLUDE "z80neo.inc"
    ORG     BIOS_BASE
    INCLUDE "bios.asm"
    INCLUDE "console.asm"
    INCLUDE "disk.asm"

; BIOS private stack (at very top of memory, grows downward)
bios_stack_area:
    DEFS    128, 0AAH
bios_stack:
