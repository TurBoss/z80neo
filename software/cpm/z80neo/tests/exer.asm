;****************************************************************************
; exer.asm — simple Z80 instruction / bus exerciser for z80neo CP/M 2.2
;
; Self-checking: every test proves a property (inverse, identity, round-trip,
; cross-check) rather than comparing against a golden CRC, so no external
; reference is needed.  It exercises the addressing modes and instruction
; classes that stress the Pico bus handler (MREQ/IORQ capture, WAIT, mux,
; MMU) so a bus fault shows up as FAIL.
;
; Build:  z88dk-z80asm -b exer.asm
;         cp exer.bin EXER.COM
; Install: cpmtools looks for ./diskdefs, so run from this directory:
;         cpmcp -f z80neo <card>/CPMDISK0.IMG EXER.COM 0:EXER.COM
; Run:    EXER  from the CCP.  ~16k bus cycles, so a couple of seconds on
;         the z80neo bus (vs hours for ZEXALL).  Prints PASS/FAIL per test
;         and a final FAILS count; any FAIL means a bus/WAIT/MMU fault.
;****************************************************************************

BDOS:       EQU     0005H
PRINT:      EQU     9
CONOUT:     EQU     2

            ORG     0100H

;==========================================================================
; Entry / test runner
;==========================================================================
start:
            LD      SP, stack
            LD      DE, msg_banner
            CALL    puts

            XOR     A
            LD      (fails), A
            LD      HL, test_table
            LD      (tptr), HL

rt_loop:
            LD      HL, (tptr)
            LD      E, (HL)
            INC     HL
            LD      D, (HL)
            INC     HL
            LD      A, D
            OR      E
            JR      Z, rt_done          ; name pointer 0 -> end of table

            LD      A, (HL)             ; routine address -> rptr
            LD      (rptr), A
            INC     HL
            LD      A, (HL)
            LD      (rptr+1), A
            INC     HL
            LD      (tptr), HL

            CALL    puts                ; print test name (DE)
            LD      HL, (rptr)
            CALL    jphl
            JR      NZ, rt_fail

            LD      DE, msg_ok
            CALL    puts
            JR      rt_loop

rt_fail:
            LD      DE, msg_fail
            CALL    puts
            LD      HL, fails
            INC     (HL)
            JR      rt_loop

rt_done:
            LD      DE, msg_done
            CALL    puts
            LD      A, (fails)
            CALL    puthex
            LD      DE, msg_crlf
            CALL    puts
            LD      A, (fails)
            OR      A
            JR      NZ, rt_exit
            LD      DE, msg_allpass
            CALL    puts
rt_exit:
            JP      0000H               ; warm boot

jphl:
            JP      (HL)

;==========================================================================
; Output helpers
;==========================================================================
; puts — print '$'-terminated string at DE via BDOS function 9
puts:
            PUSH    AF
            PUSH    BC
            PUSH    DE
            PUSH    HL
            LD      C, PRINT
            CALL    BDOS
            POP     HL
            POP     DE
            POP     BC
            POP     AF
            RET

; puthex — print A as two hex digits via BDOS function 2
puthex:
            PUSH    AF
            RRCA
            RRCA
            RRCA
            RRCA
            CALL    putdig
            POP     AF
            CALL    putdig
            RET
putdig:
            AND     0FH
            ADD     A, '0'
            CP      '9'+1
            JR      C, putdig_out
            ADD     A, 7
putdig_out:
            LD      E, A
            PUSH    AF
            PUSH    BC
            PUSH    DE
            PUSH    HL
            LD      C, CONOUT
            CALL    BDOS
            POP     HL
            POP     DE
            POP     BC
            POP     AF
            RET

;==========================================================================
; Tests — each returns Z (A=0) on pass, NZ (A=1) on fail
;==========================================================================

;--------------------------------------------------------------------------
; MEM — memory write/read across the whole buffer: catches dropped bus
; bits and address aliasing.
;--------------------------------------------------------------------------
t_mem:
            LD      HL, buf1
            LD      B, 0
            LD      A, 0
tm_fill:
            LD      (HL), A
            INC     HL
            INC     A
            DJNZ    tm_fill
            LD      HL, buf1
            LD      B, 0
            LD      A, 0
tm_ver:
            CP      (HL)
            JP      NZ, tm_bad
            INC     HL
            INC     A
            DJNZ    tm_ver

            LD      HL, buf1            ; walking bit
            LD      B, 8
            LD      C, 1
tm_walk:
            LD      (HL), C
            LD      A, (HL)
            CP      C
            JP      NZ, tm_bad
            INC     HL
            RLC     C
            DJNZ    tm_walk

            LD      HL, buf1            ; AA fill + verify
            LD      B, 0
            LD      A, 0AAH
tm_aa:
            LD      (HL), A
            INC     HL
            DJNZ    tm_aa
            LD      HL, buf1
            LD      B, 0
            LD      A, 0AAH
tm_aa_ver:
            CP      (HL)
            JP      NZ, tm_bad
            INC     HL
            DJNZ    tm_aa_ver

            LD      HL, buf1            ; 55 fill + verify
            LD      B, 0
            LD      A, 055H
tm_55a:
            LD      (HL), A
            INC     HL
            DJNZ    tm_55a
            LD      HL, buf1
            LD      B, 0
            LD      A, 055H
tm_55b:
            CP      (HL)
            JP      NZ, tm_bad
            INC     HL
            DJNZ    tm_55b
            XOR     A
            RET
tm_bad:
            LD      A, 1
            OR      A
            RET

;--------------------------------------------------------------------------
; ADDR — every addressing mode must reach the same byte.
;--------------------------------------------------------------------------
t_addr:
            LD      A, 03CH
            LD      (buf1), A
            LD      A, (buf1)
            CP      03CH
            JP      NZ, ta_bad

            LD      HL, buf1
            LD      (HL), 05AH
            LD      A, (HL)
            CP      05AH
            JP      NZ, ta_bad

            LD      BC, buf1            ; via BC
            LD      A, (BC)
            CP      05AH
            JP      NZ, ta_bad
            LD      A, 0A5H
            LD      (BC), A
            LD      A, (HL)
            CP      0A5H
            JP      NZ, ta_bad

            LD      DE, buf1            ; via DE
            LD      A, (DE)
            CP      0A5H
            JP      NZ, ta_bad

            LD      IX, buf1            ; via (IX+d)
            LD      (IX+1), 011H
            LD      A, (IX+1)
            CP      011H
            JP      NZ, ta_bad

            LD      IY, buf1            ; via (IY+d)
            LD      (IY+2), 022H
            LD      A, (IY+2)
            CP      022H
            JP      NZ, ta_bad

            LD      HL, buf1            ; cross-check via HL
            LD      A, (HL)
            CP      0A5H
            JP      NZ, ta_bad
            INC     HL
            LD      A, (HL)
            CP      011H
            JP      NZ, ta_bad
            INC     HL
            LD      A, (HL)
            CP      022H
            JP      NZ, ta_bad
            XOR     A
            RET
ta_bad:
            LD      A, 1
            OR      A
            RET

;--------------------------------------------------------------------------
; ALU8 — ADD/SUB/INC/DEC/NEG/CPL/AND/OR/XOR cross-checks.
;--------------------------------------------------------------------------
t_alu8:
            LD      A, 07FH              ; INC A == ADD A,1
            LD      B, A
            INC     B
            ADD     A, 1
            CP      B
            JP      NZ, ta8_bad

            LD      A, 080H              ; DEC A == SUB A,1
            LD      B, A
            DEC     B
            SUB     1
            CP      B
            JP      NZ, ta8_bad

            LD      A, 05AH              ; NEG twice = identity
            NEG
            NEG
            CP      05AH
            JP      NZ, ta8_bad

            LD      A, 03CH              ; CPL twice = identity
            CPL
            CPL
            CP      03CH
            JP      NZ, ta8_bad

            LD      A, 030H              ; SUB == ADD of NEG
            LD      B, 012H
            SUB     B
            LD      C, A
            LD      A, 012H
            NEG
            ADD     A, 030H
            CP      C
            JP      NZ, ta8_bad

            LD      A, 0                ; OR with A = A
            LD      B, 0ABH
            OR      B
            CP      B
            JP      NZ, ta8_bad

            LD      A, 0FFH             ; AND with A = A
            AND     B
            CP      B
            JP      NZ, ta8_bad

            LD      A, 0ABH             ; XOR with A = 0
            XOR     B
            OR      A
            JP      NZ, ta8_bad
            XOR     A
            RET
ta8_bad:
            LD      A, 1
            OR      A
            RET

;--------------------------------------------------------------------------
; ALU16 — INC/DEC rr inverse, ADD HL,rr equivalence.
;--------------------------------------------------------------------------
t_alu16:
            LD      HL, 01234H          ; INC/DEC HL round-trip
            LD      B, H
            LD      C, L
            INC     HL
            DEC     HL
            LD      A, H
            CP      B
            JP      NZ, ta16_bad
            LD      A, L
            CP      C
            JP      NZ, ta16_bad

            LD      HL, 01000H          ; ADD HL,DE == ADD HL,BC
            LD      BC, 00234H
            LD      DE, 00234H
            ADD     HL, BC
            LD      (tmp16), HL
            LD      HL, 01000H
            ADD     HL, DE
            LD      DE, (tmp16)
            LD      A, H
            CP      D
            JP      NZ, ta16_bad
            LD      A, L
            CP      E
            JP      NZ, ta16_bad
            XOR     A
            RET
ta16_bad:
            LD      A, 1
            OR      A
            RET

;--------------------------------------------------------------------------
; ROT — rotate identities, RLC/RRC inverse, RLD/RRD round-trip.
;--------------------------------------------------------------------------
t_rot:
            LD      A, 081H             ; RLCA x8 = identity
            RLCA
            RLCA
            RLCA
            RLCA
            RLCA
            RLCA
            RLCA
            RLCA
            CP      081H
            JP      NZ, tr_bad

            LD      A, 081H             ; RRCA x8 = identity
            RRCA
            RRCA
            RRCA
            RRCA
            RRCA
            RRCA
            RRCA
            RRCA
            CP      081H
            JP      NZ, tr_bad

            LD      B, 081H             ; RLC/RRC inverse
            RLC     B
            RRC     B
            LD      A, B
            CP      081H
            JP      NZ, tr_bad

            LD      HL, buf1            ; RLD then RRD = identity
            LD      (HL), 012H
            INC     HL
            LD      (HL), 034H
            LD      HL, buf1
            LD      A, 056H
            RLD
            RRD
            CP      056H
            JP      NZ, tr_bad
            LD      HL, buf1
            LD      A, (HL)
            CP      012H
            JP      NZ, tr_bad
            INC     HL
            LD      A, (HL)
            CP      034H
            JP      NZ, tr_bad
            XOR     A
            RET
tr_bad:
            LD      A, 1
            OR      A
            RET

;--------------------------------------------------------------------------
; BIT — BIT/SET/RES on memory and register.
;--------------------------------------------------------------------------
t_bit:
            LD      HL, buf1
            LD      (HL), 0
            SET     3, (HL)
            BIT     3, (HL)
            JP      Z, tbit_bad
            RES     3, (HL)
            BIT     3, (HL)
            JP      NZ, tbit_bad

            LD      B, 0
            SET     7, B
            LD      A, B
            CP      080H
            JP      NZ, tbit_bad
            BIT     7, B
            JP      Z, tbit_bad
            RES     7, B
            LD      A, B
            OR      A
            JP      NZ, tbit_bad
            XOR     A
            RET
tbit_bad:
            LD      A, 1
            OR      A
            RET

;--------------------------------------------------------------------------
; BLOCK — LDIR/LDDR/CPIR/CPDR (back-to-back bus cycles).
;--------------------------------------------------------------------------
t_block:
            LD      HL, buf1            ; buf1[i] = i
            LD      B, 64
            LD      A, 0
tbl_fill:
            LD      (HL), A
            INC     HL
            INC     A
            DJNZ    tbl_fill

            LD      HL, buf1            ; LDIR buf1 -> buf2
            LD      DE, buf2
            LD      BC, 64
            LDIR
            LD      HL, buf1
            LD      DE, buf2
            LD      B, 64
tbl_ver:
            LD      A, (DE)
            CP      (HL)
            JP      NZ, tbk_bad
            INC     HL
            INC     DE
            DJNZ    tbl_ver

            LD      HL, buf2+63         ; LDDR buf2 -> buf1
            LD      DE, buf1+63
            LD      BC, 64
            LDDR
            LD      HL, buf1
            LD      B, 64
            LD      A, 0
tbl_ver2:
            CP      (HL)
            JP      NZ, tbk_bad
            INC     HL
            INC     A
            DJNZ    tbl_ver2

            LD      HL, buf1            ; CPIR forward for 020H
            LD      BC, 64
            LD      A, 020H
            CPIR
            JP      NZ, tbk_bad

            LD      HL, buf1+63         ; CPDR backward for 020H
            LD      BC, 64
            LD      A, 020H
            CPDR
            JP      NZ, tbk_bad
            XOR     A
            RET
tbk_bad:
            LD      A, 1
            OR      A
            RET

;--------------------------------------------------------------------------
; STACK — PUSH/POP, CALL/RET, EX (SP),HL.
;--------------------------------------------------------------------------
t_stack:
            LD      HL, 01234H
            LD      DE, 05678H
            PUSH    HL
            PUSH    DE
            POP     DE
            POP     HL
            LD      A, H
            CP      012H
            JP      NZ, ts_bad
            LD      A, L
            CP      034H
            JP      NZ, ts_bad
            LD      A, D
            CP      056H
            JP      NZ, ts_bad
            LD      A, E
            CP      078H
            JP      NZ, ts_bad

            CALL    tsub
            LD      A, B
            CP      099H
            JP      NZ, ts_bad

            LD      HL, 0AAAAH          ; EX (SP),HL
            LD      DE, 0BBBBH
            PUSH    DE
            EX      (SP), HL
            POP     DE
            LD      A, H
            CP      0BBH
            JP      NZ, ts_bad
            LD      A, L
            CP      0BBH
            JP      NZ, ts_bad
            LD      A, D
            CP      0AAH
            JP      NZ, ts_bad
            LD      A, E
            CP      0AAH
            JP      NZ, ts_bad
            XOR     A
            RET
tsub:
            LD      B, 099H
            RET
ts_bad:
            LD      A, 1
            OR      A
            RET

;--------------------------------------------------------------------------
; EXCH — EX DE,HL / EXX / EX AF,AF' round-trips.
;--------------------------------------------------------------------------
t_exch:
            LD      DE, 01234H
            LD      HL, 05678H
            EX      DE, HL
            EX      DE, HL
            LD      A, D
            CP      012H
            JP      NZ, te_bad
            LD      A, H
            CP      056H
            JP      NZ, te_bad

            LD      BC, 01111H
            LD      DE, 02222H
            LD      HL, 03333H
            EXX
            LD      BC, 04444H
            LD      DE, 05555H
            LD      HL, 06666H
            EXX
            LD      A, B
            CP      011H
            JP      NZ, te_bad
            LD      A, D
            CP      022H
            JP      NZ, te_bad
            LD      A, H
            CP      033H
            JP      NZ, te_bad

            LD      A, 05AH
            EX      AF, AF'
            EX      AF, AF'
            CP      05AH
            JP      NZ, te_bad
            XOR     A
            RET
te_bad:
            LD      A, 1
            OR      A
            RET

;--------------------------------------------------------------------------
; INDEX — DD/FD prefixed loads and ADD IX/IY,rr.
;--------------------------------------------------------------------------
t_index:
            LD      IX, buf1
            LD      IY, buf2
            LD      (IX+0), 011H
            LD      (IX+7), 022H
            LD      A, (IX+0)
            CP      011H
            JP      NZ, ti_bad
            LD      A, (IX+7)
            CP      022H
            JP      NZ, ti_bad

            LD      (IY+0), 033H
            LD      (IY+7), 044H
            LD      A, (IY+0)
            CP      033H
            JP      NZ, ti_bad
            LD      A, (IY+7)
            CP      044H
            JP      NZ, ti_bad

            LD      HL, buf1            ; cross-check via HL
            LD      A, (HL)
            CP      011H
            JP      NZ, ti_bad

            LD      IX, 01000H          ; ADD IX,BC
            LD      BC, 00234H
            ADD     IX, BC
            PUSH    IX
            POP     HL
            LD      A, H
            CP      012H
            JP      NZ, ti_bad
            LD      A, L
            CP      034H
            JP      NZ, ti_bad

            LD      IY, 01000H          ; ADD IY,DE
            LD      DE, 00234H
            ADD     IY, DE
            PUSH    IY
            POP     HL
            LD      A, H
            CP      012H
            JP      NZ, ti_bad
            LD      A, L
            CP      034H
            JP      NZ, ti_bad
            XOR     A
            RET
ti_bad:
            LD      A, 1
            OR      A
            RET

;--------------------------------------------------------------------------
; DAA — BCD addition vectors.
;--------------------------------------------------------------------------
t_daa:
            LD      A, 009H
            ADD     A, 1
            DAA
            CP      010H
            JP      NZ, td_bad

            LD      A, 019H
            ADD     A, 1
            DAA
            CP      020H
            JP      NZ, td_bad

            LD      A, 099H
            ADD     A, 1
            DAA
            JP      NC, td_bad          ; carry must be set
            CP      000H
            JP      NZ, td_bad
            XOR     A
            RET
td_bad:
            LD      A, 1
            OR      A
            RET

;--------------------------------------------------------------------------
; FLAGS — documented flag bits (XF/YF masked out) for ADD/SUB/CP.
;--------------------------------------------------------------------------
t_flags:
            LD      A, 07FH             ; 7F+1 -> S,V,H
            ADD     A, 1
            PUSH    AF
            POP     BC
            LD      A, C
            AND     0D7H
            CP      094H
            JP      NZ, tf_bad

            LD      A, 00FH             ; 0F+1 -> H
            ADD     A, 1
            PUSH    AF
            POP     BC
            LD      A, C
            AND     0D7H
            CP      010H
            JP      NZ, tf_bad

            LD      A, 000H             ; 00-1 -> S,H,N,C
            SUB     1
            PUSH    AF
            POP     BC
            LD      A, C
            AND     0D7H
            CP      093H
            JP      NZ, tf_bad

            LD      A, 080H             ; 80+80 -> Z,V,C
            ADD     A, 080H
            PUSH    AF
            POP     BC
            LD      A, C
            AND     0D7H
            CP      045H
            JP      NZ, tf_bad

            LD      A, 5               ; CP equal -> Z,N
            CP      5
            PUSH    AF
            POP     BC
            LD      A, C
            AND     0D7H
            CP      042H
            JP      NZ, tf_bad
            XOR     A
            RET
tf_bad:
            LD      A, 1
            OR      A
            RET

;==========================================================================
; Strings and data
;==========================================================================
msg_banner: DB      0DH, 0AH, "z80neo Z80 exerciser", 0DH, 0AH, "$"
msg_ok:     DB      " OK", 0DH, 0AH, "$"
msg_fail:   DB      " FAIL", 0DH, 0AH, "$"
msg_done:   DB      "DONE  FAILS=$"
msg_crlf:   DB      0DH, 0AH, "$"
msg_allpass:DB      "ALL PASS", 0DH, 0AH, "$"

t_mem_name:     DB  "MEM   $"
t_addr_name:    DB  "ADDR  $"
t_alu8_name:    DB  "ALU8  $"
t_alu16_name:   DB  "ALU16 $"
t_rot_name:     DB  "ROT   $"
t_bit_name:     DB  "BIT   $"
t_block_name:   DB  "BLOCK $"
t_stack_name:   DB  "STACK $"
t_exch_name:    DB  "EXCH  $"
t_index_name:   DB  "INDEX $"
t_daa_name:     DB  "DAA   $"
t_flags_name:   DB  "FLAGS $"

test_table:
            DW  t_mem_name,   t_mem
            DW  t_addr_name,  t_addr
            DW  t_alu8_name,  t_alu8
            DW  t_alu16_name, t_alu16
            DW  t_rot_name,   t_rot
            DW  t_bit_name,   t_bit
            DW  t_block_name, t_block
            DW  t_stack_name, t_stack
            DW  t_exch_name,  t_exch
            DW  t_index_name, t_index
            DW  t_daa_name,   t_daa
            DW  t_flags_name, t_flags
            DW  0

tptr:       DW  0
rptr:       DW  0
fails:      DB  0
tmp16:      DW  0

buf1:       DEFS    256
buf2:       DEFS    256
            DEFS    64
stack:
