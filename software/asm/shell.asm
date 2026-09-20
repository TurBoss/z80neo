; shell.asm — standalone shell for z80neo (ORG 0x4000)
; Bootloader maps page 1 → physical page 2 and jumps to 0x4000

UART_DATA equ 0x80
UART_STAT equ 0x81
FLASH_D   equ 0xE0
FLASH_AL  equ 0xE1
FLASH_AM  equ 0xE2
FLASH_AH  equ 0xE3

    org 0x4000

start:
    ld sp, 0x7FF0
    ld hl, banner
    call print
    call newline

prompt:
    ld a, '>'
    call tx
    ld a, ' '
    call tx

    ; Read line into buffer
    ld hl, linebuf
    ld b, 64
rdln:
    call rx              ; wait for char
    cp 13                 ; CR = enter
    jr z, done
    cp 10                 ; LF = enter
    jr z, done
    cp 8                  ; backspace
    jr z, bksp
    cp 127                ; DEL = backspace
    jr z, bksp
    ld (hl), a
    inc hl
    call tx               ; echo
    dec b
    jr nz, rdln
done:
    ld (hl), 0
    call newline
    ld hl, linebuf
    call docmd
    jr prompt

bksp:
    push de; ld de, linebuf; or a; sbc hl, de; pop de; jr z, rdln; add hl, de; dec hl
    jr z, rdln
    dec hl
    ld a, 8
    call tx
    ld a, ' '
    call tx
    ld a, 8
    call tx
    jr rdln

; ---- Command dispatcher ----
docmd:
    ld a, (hl)
    or a
    ret z
    cp ' '
    jr z, docmd_skip
    ; Check commands
    ld de, cmdh
    call strcmp
    jp z, cmd_help
    ld de, cmdv
    call strcmp
    jp z, cmd_ver
    ld de, cmdp
    call strcmp
    jp z, cmd_peek
    ld de, cmdd
    call strcmp
    jp z, cmd_dump
    ld de, cmdw
    call strcmp
    jp z, cmd_write
    ld de, cmdr
    call strcmp
    jp z, cmd_run
    ; Unknown
    ld hl, msg_unknown
    call print
    call newline
    ret
docmd_skip:
    inc hl
    jr docmd

; ---- Commands ----
cmd_help:
    ld hl, msg_help
    call print
    ret
cmd_ver:
    ld hl, msg_ver
    call print
    ret
cmd_peek:
    call skip_sp
    call parse_hex
    ld a, (hl)
    call tx_hex
    call newline
    ret
cmd_dump:
    call skip_sp
    call parse_hex
    push hl
    ld b, 16
dloop:
    ld a, (hl)
    call tx_hex
    ld a, ' '
    call tx
    inc hl
    djnz dloop
    pop hl
    call newline
    ret
cmd_write:
    call skip_sp
    call parse_hex
    push hl
    call skip_sp
    call parse_hex
    ld (hl), a
    pop hl
    ret
cmd_run:
    call skip_sp
    call parse_hex
    jp (hl)

; ---- Parse hex ----
parse_hex:
    ld a, (hl)
    cp ' '
    jr z, parse_hex
    or a
    ret z
parse_loop:
    ld a, (hl)
    call hexval
    rl d
    rl d
    rl d
    rl d
    or d
    ld d, a
    inc hl
    ld a, (hl)
    cp ' '
    jr nz, parse_loop
    inc hl
    ld a, d
    ret

skip_sp:
    ld a, (hl)
    cp ' '
    ret nz
    inc hl
    jr skip_sp

hexval:
    cp '0'
    jr c, hex_err
    cp '9'+1
    jr c, hex09
    cp 'A'
    jr c, hex_err
    cp 'F'+1
    jr c, hexAF
    cp 'a'
    jr c, hex_err
    cp 'f'+1
    jr nc, hex_err
    sub 'a'-10
    ret
hex09: sub '0'; ret
hexAF: sub 'A'-10; ret
hex_err: xor a; ret

; ---- String compare ----
strcmp:
    push hl
sloop:
    ld a, (de)
    or a
    jr z, scmp_end
    cp (hl)
    jr nz, scmp_no
    inc hl
    inc de
    jr sloop
scmp_end:
    ld a, (hl)
    cp ' '
    jr z, scmp_yes
    or a
    jr z, scmp_yes
scmp_no:
    pop hl
    ld a, 1
    or a
    ret
scmp_yes:
    pop hl
    xor a
    ret

; ---- UART I/O ----
tx:
    push af
txw:
    in a, (UART_STAT)
    and 0x02
    jr z, txw
    pop af
    out (UART_DATA), a
    ret

rx:
    push af
rxw:
    in a, (UART_STAT)
    and 0x01
    jr z, rxw
    pop af
    in a, (UART_DATA)
    ret

tx_hex:
    push af
    rra; rra; rra; rra
    call tx_nib
    pop af
tx_nib:
    and 0x0F
    add '0'
    cp '9'+1
    jr c, tx_dig
    add 7
tx_dig:
    call tx
    ret

print:
    ld a, (hl)
    or a
    ret z
    call tx
    inc hl
    jr print

newline:
    ld a, 13; call tx
    ld a, 10; call tx
    ret

; ---- Data ----
banner:   defm "z80neo Shell v0.1 - TurBoss 2026", 0
msg_unknown: defm "?", 0
msg_help: defm "help ver peek dump write run", 13, 10
          defm "  peek ADDR - read byte", 13, 10
          defm "  dump ADDR - dump 16 bytes", 13, 10
          defm "  write ADDR VAL - write byte", 13, 10
          defm "  run ADDR - jump to address", 13, 10, 0
msg_ver:  defm "v0.1", 13, 10, 0

cmdh: defm "help", 0
cmdv: defm "ver", 0
cmdp: defm "peek", 0
cmdd: defm "dump", 0
cmdw: defm "write", 0
cmdr: defm "run", 0

linebuf: defs 64
