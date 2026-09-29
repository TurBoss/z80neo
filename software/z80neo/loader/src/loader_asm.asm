;
; loader_asm.asm — jump + page-0 relocation support for the z80neo loader.
;
; The C loader fills the globals below, then calls one of:
;
;   loader_start     — set SP and jump to boot_entry (image at 0x4000+).
;   loader_relocate  — copy a position-independent copier to page 2 (0x8000),
;                      set up DE=dst, HL=src, BC=len, IY=entry, SP, and jump
;                      there; the copier copies the image down over page 0.
;
; The copier deliberately runs from 0x8000 (page 2) so it survives being
; overwritten, and it touches no stack, so clobbering page 0 is safe.  Before
; copying it remaps MMU page 0 to 0x34, which aliases bank 0 (the firmware's
; default) while dodging its reset-vector write-protect, which matches the
; register value 0x20 exactly.
;

    module  loader_asm

    defc    COPY_RAM = 0x8000
    defc    RELOC_LEN = _reloc_copier_end - _reloc_copier_src

    SECTION code_user

    PUBLIC  _loader_start
    PUBLIC  _loader_relocate

    EXTERN  _boot_entry
    EXTERN  _boot_sp
    EXTERN  _rel_entry
    EXTERN  _rel_sp
    EXTERN  _rel_src
    EXTERN  _rel_dst
    EXTERN  _rel_len

; ---------------------------------------------------------------------------
; void loader_start(void) — set SP, jump to boot_entry.  Never returns.
; ---------------------------------------------------------------------------
_loader_start:
    ld      sp, (_boot_sp)
    ld      hl, (_boot_entry)
    jp      (hl)

; ---------------------------------------------------------------------------
; void loader_relocate(void) — stage a page-0 image.  Never returns.
; ---------------------------------------------------------------------------
_loader_relocate:
    ld      hl, _reloc_copier_src
    ld      de, COPY_RAM
    ld      bc, RELOC_LEN
    ldir

    ld      sp, (_rel_sp)
    ld      iy, (_rel_entry)
    ld      hl, (_rel_src)
    ld      de, (_rel_dst)
    ld      bc, (_rel_len)
    jp      COPY_RAM

; ---------------------------------------------------------------------------
; Relocatable copier: DE=dst, HL=src, BC=len, IY=entry.
; ---------------------------------------------------------------------------
_reloc_copier_src:
    in      a, (0xF0)
    cp      0x20
    jr      nz, rc_go
    ld      a, 0x34
    out     (0xF0), a
rc_go:
    ldir
    jp      (iy)
_reloc_copier_end:
