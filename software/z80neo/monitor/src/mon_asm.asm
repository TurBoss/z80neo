;
; mon_asm.asm — low-level support for the z80neo monitor.
;
; The monitor runs bare-metal on the Z80 (zcc +z80neo), owns page 0 and
; talks to the console on ports 0x80/0x81.  This module provides:
;
;   _main            entry point: switches to the dedicated monitor stack
;                    and calls the C monitor_body(), then halts.
;   _mon_run         load user registers from _mon_regs and jump to PC.
;                    Returns to the caller when asm_im1_handler traps.
;   asm_im1_handler  RST 38h trap: snapshot the whole CPU into _mon_regs,
;                    then return to _mon_run's trap_return label.
;   _mon_mem_top     first byte of free RAM after the monitor image.
;
; Trap mechanism: the C side writes 0xFF (RST 38h) into the breakpoint /
; next-instruction address.  The CRT0 installs `jp asm_im1_handler` at
; 0x0038, so this handler overrides the classic library's default IM1
; handler.  See monitor.c for the breakpoint bookkeeping.
;

    defc MON_STACK_SIZE = 512

    SECTION code_user

    PUBLIC  _main
    PUBLIC  _mon_run
    PUBLIC  _mon_mem_top
    PUBLIC  asm_im1_handler

    EXTERN  _monitor_body
    EXTERN  _mon_regs
    EXTERN  __BSS_END_tail

; ---------------------------------------------------------------------------
; _main — CRT entry.  The CRT leaves SP at 0x3fff; move to the monitor stack
; (a fixed low-RAM area) so the user program can use the top of memory freely.
; ---------------------------------------------------------------------------
_main:
    ld      sp, _mon_stack_top
    call    _monitor_body
    halt

; ---------------------------------------------------------------------------
; unsigned int mon_mem_top(void) — end of the monitor (BSS tail) = first free
; byte.  Programs loaded below this would clobber the monitor.
; ---------------------------------------------------------------------------
_mon_mem_top:
    ld      hl, __BSS_END_tail
    ret

; ---------------------------------------------------------------------------
; void mon_run(void)
;   Load AF/BC/DE/HL/AF'/BC'/DE'/HL'/IX/IY from _mon_regs, set SP from
;   _mon_regs[R_SP] and jump to _mon_regs[R_PC].  Returns only when a RST 38h
;   trap fires (the handler restores the monitor stack and returns through
;   mon_trap_return).
; ---------------------------------------------------------------------------
_mon_run:
    push    ix
    push    iy
    ld      hl, mon_trap_return
    push    hl
    ld      (_mon_sp_save), sp

    ; Patch the operand of the jump below with the user PC.  This keeps HL
    ; free to hold the user's HL value (there is no jp (nn)).
    ld      a, (_mon_regs + 22)
    ld      (mon_jp + 1), a
    ld      a, (_mon_regs + 23)
    ld      (mon_jp + 2), a

    ; alternate register set
    ld      hl, (_mon_regs + 8)         ; AF'
    push    hl
    pop     af
    ex      af, af'
    ld      bc, (_mon_regs + 10)        ; BC'
    ld      de, (_mon_regs + 12)        ; DE'
    ld      hl, (_mon_regs + 14)        ; HL'
    exx

    ; index registers
    ld      ix, (_mon_regs + 16)
    ld      iy, (_mon_regs + 18)

    ; main register set
    ld      bc, (_mon_regs + 2)
    ld      de, (_mon_regs + 4)
    ld      hl, (_mon_regs + 0)         ; AF (A high, F low)
    push    hl
    pop     af
    ld      hl, (_mon_regs + 6)         ; HL last: must survive

    ld      sp, (_mon_regs + 20)        ; user stack
mon_jp:
    jp      0                           ; operand patched above

mon_trap_return:
    pop     iy
    pop     ix
    ret

; ---------------------------------------------------------------------------
; asm_im1_handler — RST 38h trap.  Entry: SP -> return address pushed by RST.
; All registers are copied to _mon_regs, then control returns through the
; monitor stack to mon_trap_return.
; ---------------------------------------------------------------------------
asm_im1_handler:
    ld      (_mon_regs + 6), hl         ; HL
    ld      (_mon_regs + 2), bc         ; BC
    ld      (_mon_regs + 4), de         ; DE
    push    af                          ; AF has no ld (nn),af
    pop     hl
    ld      (_mon_regs + 0), hl

    pop     hl                          ; return address = trap address + 1
    dec     hl
    ld      (_mon_regs + 22), hl        ; PC = trap address
    ld      (_mon_regs + 20), sp        ; user SP

    exx                                 ; alternate set
    ld      (_mon_regs + 14), hl        ; HL'
    ld      (_mon_regs + 10), bc        ; BC'
    ld      (_mon_regs + 12), de        ; DE'
    exx
    ex      af, af'
    push    af
    pop     hl
    ld      (_mon_regs + 8), hl         ; AF'
    ex      af, af'

    ld      (_mon_regs + 16), ix        ; IX
    ld      (_mon_regs + 18), iy        ; IY

    ld      a, i
    ld      (_mon_regs + 24), a         ; I
    ld      a, r
    ld      (_mon_regs + 25), a         ; R

    ld      sp, (_mon_sp_save)
    ret

; ---------------------------------------------------------------------------
; Fallback trap target used before mon_run has ever been entered (for a
; stray RST 38h).  Restart the monitor on its own stack.
; ---------------------------------------------------------------------------
mon_default_trap:
    ld      sp, _mon_stack_top
    jp      _monitor_body

; ---------------------------------------------------------------------------
; Data / BSS
; ---------------------------------------------------------------------------
    SECTION data_user

    PUBLIC  _mon_sp_save
_mon_sp_save:
    defw    mon_default_sp

mon_default_sp:
    defw    mon_default_trap

    SECTION bss_user

_mon_stack:
    defs    MON_STACK_SIZE
    PUBLIC  _mon_stack_top
_mon_stack_top:
