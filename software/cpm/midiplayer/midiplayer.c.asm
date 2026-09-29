;* * * * *  Small-C/Plus z88dk * * * * *
;  Version: 26002-aec1401834-20260916
;
;	Reconstructed for z80 Module Assembler
;
;	Module compile time: Fri Sep 25 00:36:14 2026


	C_LINE	0,"software/cpm/midiplayer/midiplayer.c"

	MODULE	software_cpm_midiplayer_midiplayer_c


	INCLUDE "z80_crt0.hdr"


	C_LINE	0,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	0,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/sys/compiler.h"
	C_LINE	0,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/sys/proto.h"
	C_LINE	6,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/sys/compiler.h"
	C_LINE	13,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	0,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/sys/types.h"
	C_LINE	17,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/sys/types.h"
	C_LINE	26,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/sys/types.h"
	C_LINE	38,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/sys/types.h"
	C_LINE	44,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/sys/types.h"
	C_LINE	49,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/sys/types.h"
	C_LINE	54,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/sys/types.h"
	C_LINE	59,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/sys/types.h"
	C_LINE	64,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/sys/types.h"
	C_LINE	69,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/sys/types.h"
	C_LINE	74,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/sys/types.h"
	C_LINE	79,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/sys/types.h"
	C_LINE	84,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/sys/types.h"
	C_LINE	89,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/sys/types.h"
	C_LINE	93,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/sys/types.h"
	C_LINE	94,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/sys/types.h"
	C_LINE	95,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/sys/types.h"
	C_LINE	97,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/sys/types.h"
	C_LINE	98,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/sys/types.h"
	C_LINE	99,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/sys/types.h"
	C_LINE	105,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/sys/types.h"
	C_LINE	110,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/sys/types.h"
	C_LINE	14,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	0,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/stdint.h"
	C_LINE	13,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/stdint.h"
	C_LINE	14,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/stdint.h"
	C_LINE	15,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/stdint.h"
	C_LINE	17,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/stdint.h"
	C_LINE	18,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/stdint.h"
	C_LINE	19,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/stdint.h"
	C_LINE	21,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/stdint.h"
	C_LINE	22,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/stdint.h"
	C_LINE	23,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/stdint.h"
	C_LINE	25,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/stdint.h"
	C_LINE	26,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/stdint.h"
	C_LINE	27,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/stdint.h"
	C_LINE	29,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/stdint.h"
	C_LINE	30,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/stdint.h"
	C_LINE	31,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/stdint.h"
	C_LINE	33,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/stdint.h"
	C_LINE	34,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/stdint.h"
	C_LINE	35,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/stdint.h"
	C_LINE	37,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/stdint.h"
	C_LINE	38,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/stdint.h"
	C_LINE	40,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/stdint.h"
	C_LINE	41,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/stdint.h"
	C_LINE	43,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/stdint.h"
	C_LINE	44,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/stdint.h"
	C_LINE	49,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/stdint.h"
	C_LINE	52,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/stdint.h"
	C_LINE	54,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/stdint.h"
	C_LINE	55,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/stdint.h"
	C_LINE	15,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	21,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	52,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	80,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	95,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	107,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	121,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	166,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	167,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	170,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	171,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	215,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	217,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	222,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	225,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	226,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	229,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	230,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	233,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	236,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	239,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	252,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	253,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	254,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	257,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	258,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	260,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	261,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	262,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	263,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	264,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	265,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	273,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	275,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	277,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	279,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	281,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	283,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	285,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	288,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	295,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	297,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	300,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	302,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	304,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	314,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	323,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	389,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	392,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	396,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	399,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	402,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	405,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	410,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	416,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	417,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	421,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	422,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	426,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	427,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	432,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	567,"/home/turboss/Dev/PICO/z80neo/software/z88dk/lib/config/../..//include/cpm.h"
	C_LINE	35,"software/cpm/midiplayer/midiplayer.c"
	C_LINE	40,"software/cpm/midiplayer/midiplayer.c"
	C_LINE	41,"software/cpm/midiplayer/midiplayer.c"
	C_LINE	42,"software/cpm/midiplayer/midiplayer.c"
	C_LINE	43,"software/cpm/midiplayer/midiplayer.c"
	C_LINE	45,"software/cpm/midiplayer/midiplayer.c"
	C_LINE	46,"software/cpm/midiplayer/midiplayer.c"
	C_LINE	47,"software/cpm/midiplayer/midiplayer.c"
	C_LINE	48,"software/cpm/midiplayer/midiplayer.c"
	C_LINE	72,"software/cpm/midiplayer/midiplayer.c"
	C_LINE	73,"software/cpm/midiplayer/midiplayer.c"
	C_LINE	75,"software/cpm/midiplayer/midiplayer.c"
	C_LINE	79,"software/cpm/midiplayer/midiplayer.c"
	SECTION	code_compiler

; Function con_put flags 0x00000200 __smallc 
; void con_put(unsigned char c)
; parameter 'unsigned char c' at sp+2 size(1)
	C_LINE	79,"software/cpm/midiplayer/midiplayer.c::con_put::0::0"
._con_put
	ld	hl,6	;const
	push	hl
	ld	l,4
	add	hl,sp
	ld	l,(hl)
	ld	h,0
	push	hl
	call	bdos_callee
	ret


	C_LINE	81,"software/cpm/midiplayer/midiplayer.c::con_put::0::1"

; Function con_puts flags 0x00000200 __smallc 
; void con_puts(const char * s)
; parameter 'const char * s' at sp+2 size(2)
	C_LINE	81,"software/cpm/midiplayer/midiplayer.c::con_puts::0::1"
._con_puts
.i_2
	pop	bc
	pop	hl
	push	hl
	push	bc
	ld	a,(hl)
	and	a
	ret	z
	pop	de
	pop	hl
	inc	hl
	push	hl
	push	de
	dec	hl
	ld	l,(hl)
	ld	h,0
	push	hl
	call	_con_put
	pop	bc
	jp	i_2	;EOS
.i_3
	ret


	C_LINE	87,"software/cpm/midiplayer/midiplayer.c::con_puts::0::2"

; Function con_get flags 0x00000200 __smallc 
; int con_get()
	C_LINE	87,"software/cpm/midiplayer/midiplayer.c::con_get::0::2"
._con_get
	ld	hl,6	;const
	push	hl
	ld	l,255
	push	hl
	call	bdos_callee
	ret


	C_LINE	89,"software/cpm/midiplayer/midiplayer.c::con_get::0::3"

; Function con_num flags 0x00000200 __smallc 
; void con_num(unsigned int v)
; parameter 'unsigned int v' at sp+2 size(2)
	C_LINE	89,"software/cpm/midiplayer/midiplayer.c::con_num::0::3"
._con_num
	push	bc
	push	bc
	push	bc
	dec	sp
	pop	hl
	ld	l,0
	push	hl
	ld	hl,9	;const
	add	hl,sp
	call	l_gint	;
	ld	a,h
	or	l
	jp	nz,i_4	;
	ld	hl,48	;const
	push	hl
	call	_con_put
	pop	bc
	inc	sp
	pop	bc
	pop	bc
	pop	bc
	ret


.i_4
	ld	hl,9	;const
	add	hl,sp
	call	l_gint	;
	ld	a,h
	or	l
	jp	z,i_6	;
	ld	hl,1	;const
	add	hl,sp
	push	hl
	dec	hl
	inc	(hl)
	ld	l,(hl)
	ld	h,0
	dec	l
	pop	de
	add	hl,de
	push	hl
	ld	hl,11	;const
	add	hl,sp
	ld	e,(hl)
	inc	hl
	ld	d,(hl)
	ld	hl,10
	call	l_div_u
	ex	de,hl
	ld	bc,48
	add	hl,bc
	ld	a,l
	call	l_sxt
	ld	a,l
	pop	de
	ld	(de),a
	ld	hl,9	;const
	add	hl,sp
	push	hl
	ld	e,(hl)
	inc	hl
	ld	d,(hl)
	ld	hl,10
	call	l_div_u
	pop	de
	call	l_pint
	jp	i_5	;EOS
.i_6
	ld	hl,0	;const
	add	hl,sp
	ld	a,(hl)
	and	a
	jp	z,i_8	;
	ld	hl,1	;const
	add	hl,sp
	push	hl
	dec	hl
	dec	(hl)
	ld	l,(hl)
	ld	h,0
	pop	de
	add	hl,de
	ld	l,(hl)
	ld	h,0
	push	hl
	call	_con_put
	pop	bc
	jp	i_7	;EOS
.i_8
	inc	sp
	pop	bc
	pop	bc
	pop	bc
	ret


	C_LINE	105,"software/cpm/midiplayer/midiplayer.c::con_num::0::6"

; Function con_hex8 flags 0x00000200 __smallc 
; void con_hex8(unsigned char v)
; parameter 'unsigned char v' at sp+2 size(1)
	C_LINE	105,"software/cpm/midiplayer/midiplayer.c::con_hex8::0::6"
._con_hex8
	SECTION	rodata_compiler
._st_con_hex8_hex
	defm	"0123456789ABCDEF"
	defb	0

	SECTION	code_compiler
	ld	hl,_st_con_hex8_hex
	push	hl
	ld	hl,4	;const
	add	hl,sp
	ld	l,(hl)
	ld	h,0
	ld	a,l
	rrca
	rrca
	rrca
	rrca
	and	15
	ld	l,a
	pop	de
	add	hl,de
	ld	l,(hl)
	ld	h,0
	push	hl
	call	_con_put
	pop	bc
	ld	hl,_st_con_hex8_hex
	push	hl
	ld	hl,4	;const
	add	hl,sp
	ld	a,(hl)
	and	15
	ld	l,a
	ld	h,0
	pop	de
	add	hl,de
	ld	l,(hl)
	ld	h,0
	push	hl
	call	_con_put
	pop	bc
	ret


	C_LINE	113,"software/cpm/midiplayer/midiplayer.c::con_hex8::0::7"
	C_LINE	116,"software/cpm/midiplayer/midiplayer.c::con_hex8::0::7"

; Function read_line flags 0x00000200 __smallc 
; void read_line(char * buf, unsigned char max)
; parameter 'unsigned char max' at sp+2 size(1)
; parameter 'char * buf' at sp+4 size(2)
	C_LINE	116,"software/cpm/midiplayer/midiplayer.c::read_line::0::7"
._read_line
	dec	sp
	pop	hl
	ld	l,0
	push	hl
	push	bc
.i_10
	call	_con_get
	pop	bc
	push	hl
	ld	a,h
	or	l
	jp	nz,i_14	;
	ld	hl,(_g_wait_ticks)
	inc	hl
	ld	(_g_wait_ticks),hl
	jp	i_13	;EOS
.i_14
	pop	hl
	push	hl
	ld	de,13
	and	a
	sbc	hl,de
	jp	z,i_16	;
	pop	hl
	push	hl
	ld	de,10
	and	a
	sbc	hl,de
	jp	z,i_11	;EOS
.i_15
	pop	hl
	push	hl
	ld	de,8
	and	a
	sbc	hl,de
	jp	z,i_19	;
	pop	hl
	push	hl
	ld	de,127
	and	a
	sbc	hl,de
	jp	nz,i_18	;
.i_19
	ld	hl,2	;const
	add	hl,sp
	ld	a,(hl)
	and	a
	jp	z,i_21	;
	ld	hl,2	;const
	add	hl,sp
	dec	(hl)
	ld	hl,i_1+0
	push	hl
	call	_con_puts
	pop	bc
	jp	i_10	;EOS
.i_18
	pop	hl
	push	hl
	ld	de,32
	ex	de,hl
	call	l_ge
	jp	nc,i_23	;
	ld	hl,2	;const
	add	hl,sp
	ld	l,(hl)
	ld	h,0
	push	hl
	ld	hl,7	;const
	add	hl,sp
	ld	l,(hl)
	dec	l
	ld	h,0
	pop	de
	ex	de,hl
	and	a
	sbc	hl,de
	jp	nc,i_23	;
.i_24_i_23
	ld	hl,7	;const
	call	l_gintspsp	;
	ld	hl,4	;const
	add	hl,sp
	inc	(hl)
	ld	l,(hl)
	ld	h,0
	dec	l
	pop	de
	add	hl,de
	pop	de
	push	de
	push	hl
	ex	de,hl
	ld	a,l
	call	l_sxt
	ld	a,l
	pop	de
	ld	(de),a
	pop	hl
	push	hl
	ld	h,0
	push	hl
	call	_con_put
	pop	bc
	jp	i_10	;EOS
.i_11
	ld	hl,7	;const
	add	hl,sp
	ld	e,(hl)
	inc	hl
	ld	d,(hl)
	ld	hl,2	;const
	add	hl,sp
	ld	l,(hl)
	ld	h,0
	add	hl,de
	ld	d,h
	ld	e,l
	xor	a
	ld	(de),a
	ld	hl,i_1+4
	push	hl
	call	_con_puts
	pop	bc
	inc	sp
	pop	bc
	ret


	C_LINE	143,"software/cpm/midiplayer/midiplayer.c::read_line::0::12"
	C_LINE	144,"software/cpm/midiplayer/midiplayer.c::read_line::0::12"
	C_LINE	145,"software/cpm/midiplayer/midiplayer.c::read_line::0::12"
	C_LINE	147,"software/cpm/midiplayer/midiplayer.c::read_line::0::12"

; Function midi_init flags 0x00000200 __smallc 
; void midi_init()
	C_LINE	147,"software/cpm/midiplayer/midiplayer.c::midi_init::0::12"
._midi_init
	in	a,(_MIDI_CFG_BAUD)
	ld	l,a
	ld	h,0
	ld	(_g_old_baud),a
	in	a,(_MIDI_CFG_FMT)
	ld	hl,_g_old_fmt
	ld	(hl),a
	and	128
	ld	l,a
	ld	h,0
	and	a
	ld	hl,0	;const
	jr	z,ASMPC+3
	inc	hl
	ld	a,l
	ld	(_g_have_cfg),a
	ld	hl,3	;const
	ld	a,l
	out	(_MIDI_CFG_FMT),a
	ld	hl,10	;const
	ld	a,l
	out	(_MIDI_CFG_BAUD),a
	ret


	C_LINE	159,"software/cpm/midiplayer/midiplayer.c::midi_init::0::13"

; Function midi_restore flags 0x00000200 __smallc 
; void midi_restore()
	C_LINE	159,"software/cpm/midiplayer/midiplayer.c::midi_restore::0::13"
._midi_restore
	ld	a,(_g_have_cfg)
	and	a
	jp	nz,i_25	;
	ret


.i_25
	ld	hl,(_g_old_fmt)
	ld	h,0
	ld	a,l
	out	(_MIDI_CFG_FMT),a
	ld	hl,(_g_old_baud)
	ld	h,0
	ld	a,l
	out	(_MIDI_CFG_BAUD),a
	ret


	C_LINE	171,"software/cpm/midiplayer/midiplayer.c::midi_restore::0::14"
	C_LINE	176,"software/cpm/midiplayer/midiplayer.c::midi_restore::0::14"

; Function dir_start flags 0x00000200 __smallc 
; unsigned char uint8_tdir_start(const char * dir, unsigned int skip)
; parameter 'unsigned int skip' at sp+2 size(2)
; parameter 'const char * dir' at sp+4 size(2)
	C_LINE	176,"software/cpm/midiplayer/midiplayer.c::dir_start::0::14"
._dir_start
	push	bc
	ld	hl,0	;const
	ld	a,l
	out	(_FILE_RST),a
	call	l_gint6sp	;
	pop	bc
	push	hl
	jp	i_28	;EOS
.i_26
	pop	hl
	inc	hl
	push	hl
.i_28
	pop	hl
	push	hl
	ld	a,(hl)
	and	a
	jp	z,i_27	;
	pop	hl
	push	hl
	ld	l,(hl)
	ld	h,0
	ld	a,l
	out	(_FILE_NAME),a
	jp	i_26	;EOS
.i_27
	ld	hl,5	;const
	ld	a,l
	out	(_FILE_CMD),a
	in	a,(_FILE_CMD)
	ld	e,a
	ld	d,0
	ld	hl,4	;const
	call	l_and
	ld	a,h
	or	l
	jp	z,i_29	;
	ld	hl,0	;const
	pop	bc
	ret


.i_29
	ld	hl,4	;const
	add	hl,sp
	dec	(hl)
	ld	a,(hl)
	inc	hl
	cp	255
	jr	nz,ASMPC+3
	dec	(hl)
	ld	h,(hl)
	ld	l,a
	inc	hl
	ld	a,h
	or	l
	jp	z,i_31	;
	ld	hl,6	;const
	ld	a,l
	out	(_FILE_CMD),a
	in	a,(_FILE_CMD)
	ld	e,a
	ld	d,0
	ld	hl,16	;const
	call	l_and
	ld	de,0	;const
	call	l_eq
	jp	nc,i_32	;
	ld	hl,0	;const
	pop	bc
	ret
.i_31
	ld	hl,1	;const
	pop	bc
	ret


	C_LINE	194,"software/cpm/midiplayer/midiplayer.c::dir_start::0::16"

; Function dir_next flags 0x00000200 __smallc 
; unsigned char uint8_tdir_next(char * out, unsigned char max, unsigned char uint8_t* is_dir)
; parameter 'unsigned char uint8_t* is_dir' at sp+2 size(2)
; parameter 'unsigned char max' at sp+4 size(1)
; parameter 'char * out' at sp+6 size(2)
	C_LINE	194,"software/cpm/midiplayer/midiplayer.c::dir_next::0::16"
._dir_next
	dec	sp
	pop	hl
	ld	l,0
	push	hl
	push	bc
	ld	hl,6	;const
	ld	a,l
	out	(_FILE_CMD),a
	ld	hl,0	;const
	add	hl,sp
	push	hl
	in	a,(_FILE_CMD)
	ld	l,a
	pop	de
	ld	(de),a
	call	l_gint5sp	;
	ld	a,h
	or	l
	jp	z,i_33	;
	ld	hl,5	;const
	call	l_gintspsp	;
	ld	hl,2	;const
	add	hl,sp
	ld	a,(hl)
	and	64
	ld	l,a
	ld	h,0
	and	a
	ld	hl,0	;const
	jr	z,ASMPC+3
	inc	hl
	pop	de
	ld	a,l
	ld	(de),a
.i_33
	pop	hl
	push	hl
	ld	h,0
	ld	a,16
	and	l
	ld	l,a
	jr	nz,ASMPC+3
	scf
	jp	nc,i_34	;
	ld	hl,0	;const
	inc	sp
	pop	bc
	ret


.i_34
	ld	hl,1	;const
	add	hl,sp
	push	hl
	in	a,(_FILE_NAME)
	ld	l,a
	pop	de
	ld	(de),a
	ld	a,l
	and	a
	jp	z,i_37	;
	ld	hl,2	;const
	add	hl,sp
	ld	l,(hl)
	ld	h,0
	push	hl
	ld	hl,9	;const
	add	hl,sp
	ld	l,(hl)
	dec	l
	ld	h,0
	pop	de
	ex	de,hl
	and	a
	sbc	hl,de
	jp	nc,i_37	;
.i_38_i_37
	ld	hl,9	;const
	call	l_gintspsp	;
	ld	hl,4	;const
	add	hl,sp
	inc	(hl)
	ld	l,(hl)
	ld	h,0
	dec	l
	pop	de
	add	hl,de
	push	hl
	ld	hl,3	;const
	add	hl,sp
	ld	l,(hl)
	ld	a,l
	rla
	sbc	a
	ld	h,a
	ld	a,l
	pop	de
	ld	(de),a
	jp	i_35	;EOS
.i_36
	ld	hl,9	;const
	add	hl,sp
	ld	e,(hl)
	inc	hl
	ld	d,(hl)
	ld	hl,2	;const
	add	hl,sp
	ld	l,(hl)
	ld	h,0
	add	hl,de
	ld	d,h
	ld	e,l
	xor	a
	ld	(de),a
	ld	hl,2	;const
	add	hl,sp
	ld	l,(hl)
	ld	h,0
	inc	sp
	pop	bc
	ret


	C_LINE	210,"software/cpm/midiplayer/midiplayer.c::dir_next::0::17"

; Function path_join flags 0x00000200 __smallc 
; void path_join(char * out, unsigned char max, const char * dir, const char * name)
; parameter 'const char * name' at sp+2 size(2)
; parameter 'const char * dir' at sp+4 size(2)
; parameter 'unsigned char max' at sp+6 size(1)
; parameter 'char * out' at sp+8 size(2)
	C_LINE	210,"software/cpm/midiplayer/midiplayer.c::path_join::0::17"
._path_join
	dec	sp
	pop	hl
	ld	l,0
	push	hl
	push	bc
	call	l_gint5sp	;
	ld	a,(hl)
	cp	47
	jp	nz,i_39	;
	call	l_gint5sp	;
	pop	bc
	push	hl
	jp	i_42	;EOS
.i_40
	pop	hl
	inc	hl
	push	hl
.i_42
	pop	hl
	push	hl
	ld	a,(hl)
	and	a
	jp	z,i_43	;
	ld	hl,2	;const
	add	hl,sp
	ld	l,(hl)
	ld	h,0
	push	hl
	ld	hl,11	;const
	add	hl,sp
	ld	l,(hl)
	dec	l
	ld	h,0
	pop	de
	ex	de,hl
	and	a
	sbc	hl,de
	jp	nc,i_43	;
.i_44_i_43
	ld	hl,11	;const
	call	l_gintspsp	;
	ld	hl,4	;const
	add	hl,sp
	inc	(hl)
	ld	l,(hl)
	ld	h,0
	dec	l
	pop	de
	add	hl,de
	pop	de
	push	de
	push	hl
	ex	de,hl
	call	l_gchar
	pop	de
	ld	a,l
	ld	(de),a
	jp	i_40	;EOS
.i_41
	ld	hl,11	;const
	add	hl,sp
	ld	e,(hl)
	inc	hl
	ld	d,(hl)
	ld	hl,2	;const
	add	hl,sp
	ld	l,(hl)
	ld	h,0
	add	hl,de
	ld	(hl),0
	ld	l,(hl)
	ld	h,0
	inc	sp
	pop	bc
	ret


.i_39
	call	l_gint7sp	;
	ld	a,(hl)
	and	a
	jp	z,i_45	;
	call	l_gint7sp	;
	pop	bc
	push	hl
	jp	i_48	;EOS
.i_46
	pop	hl
	inc	hl
	push	hl
.i_48
	pop	hl
	push	hl
	ld	a,(hl)
	and	a
	jp	z,i_49	;
	ld	hl,2	;const
	add	hl,sp
	ld	l,(hl)
	ld	h,0
	push	hl
	ld	hl,11	;const
	add	hl,sp
	ld	l,(hl)
	dec	l
	ld	h,0
	pop	de
	ex	de,hl
	and	a
	sbc	hl,de
	jp	nc,i_49	;
.i_50_i_49
	ld	hl,11	;const
	call	l_gintspsp	;
	ld	hl,4	;const
	add	hl,sp
	inc	(hl)
	ld	l,(hl)
	ld	h,0
	dec	l
	pop	de
	add	hl,de
	pop	de
	push	de
	push	hl
	ex	de,hl
	call	l_gchar
	pop	de
	ld	a,l
	ld	(de),a
	jp	i_46	;EOS
.i_47
	ld	hl,2	;const
	add	hl,sp
	ld	a,(hl)
	and	a
	jp	z,i_52	;
	ld	hl,11	;const
	call	l_gintspsp	;
	ld	hl,4	;const
	add	hl,sp
	ld	l,(hl)
	ld	h,0
	dec	hl
	pop	de
	add	hl,de
	ld	a,(hl)
	cp	47
	jp	z,i_52	;
	ld	hl,2	;const
	add	hl,sp
	ld	l,(hl)
	ld	h,0
	push	hl
	ld	hl,11	;const
	add	hl,sp
	ld	l,(hl)
	dec	l
	ld	h,0
	pop	de
	ex	de,hl
	and	a
	sbc	hl,de
	jp	nc,i_52	;
.i_53_i_52
	ld	hl,11	;const
	call	l_gintspsp	;
	ld	hl,4	;const
	add	hl,sp
	inc	(hl)
	ld	l,(hl)
	ld	h,0
	dec	l
	pop	de
	add	hl,de
	ld	(hl),47
.i_51
	call	l_gint5sp	;
	pop	bc
	push	hl
	jp	i_56	;EOS
.i_54
	pop	hl
	inc	hl
	push	hl
.i_56
	pop	hl
	push	hl
	ld	a,(hl)
	and	a
	jp	z,i_57	;
	ld	hl,2	;const
	add	hl,sp
	ld	l,(hl)
	ld	h,0
	push	hl
	ld	hl,11	;const
	add	hl,sp
	ld	l,(hl)
	dec	l
	ld	h,0
	pop	de
	ex	de,hl
	and	a
	sbc	hl,de
	jp	nc,i_57	;
.i_58_i_57
	ld	hl,11	;const
	call	l_gintspsp	;
	ld	hl,4	;const
	add	hl,sp
	inc	(hl)
	ld	l,(hl)
	ld	h,0
	dec	l
	pop	de
	add	hl,de
	pop	de
	push	de
	push	hl
	ex	de,hl
	call	l_gchar
	pop	de
	ld	a,l
	ld	(de),a
	jp	i_54	;EOS
.i_55
	ld	hl,11	;const
	add	hl,sp
	ld	e,(hl)
	inc	hl
	ld	d,(hl)
	ld	hl,2	;const
	add	hl,sp
	ld	l,(hl)
	ld	h,0
	add	hl,de
	ld	(hl),0
	ld	l,(hl)
	ld	h,0
	inc	sp
	pop	bc
	ret


	C_LINE	232,"software/cpm/midiplayer/midiplayer.c::path_join::0::20"

; Function make_path flags 0x00000200 __smallc 
; void make_path(char * out, unsigned char max, const char * name)
; parameter 'const char * name' at sp+2 size(2)
; parameter 'unsigned char max' at sp+4 size(1)
; parameter 'char * out' at sp+6 size(2)
	C_LINE	232,"software/cpm/midiplayer/midiplayer.c::make_path::0::20"
._make_path
	ld	hl,6	;const
	call	l_gintspsp	;
	ld	hl,6	;const
	add	hl,sp
	ld	l,(hl)
	ld	h,0
	push	hl
	ld	hl,_g_dir
	push	hl
	call	l_gint8sp	;
	push	hl
	call	_path_join
	pop	bc
	pop	bc
	pop	bc
	pop	bc
	ret


	C_LINE	237,"software/cpm/midiplayer/midiplayer.c::make_path::0::21"

; Function dir_up flags 0x00000200 __smallc 
; void dir_up(char * dir)
; parameter 'char * dir' at sp+2 size(2)
	C_LINE	237,"software/cpm/midiplayer/midiplayer.c::dir_up::0::21"
._dir_up
	ld	hl,0
	push	hl
.i_59
	ld	hl,4	;const
	add	hl,sp
	ld	e,(hl)
	inc	hl
	ld	d,(hl)
	pop	hl
	push	hl
	ld	l,h
	ld	h,0
	add	hl,de
	ld	a,(hl)
	and	a
	jp	z,i_60	;
	ld	hl,4	;const
	add	hl,sp
	ld	e,(hl)
	inc	hl
	ld	d,(hl)
	pop	hl
	push	hl
	ld	l,h
	ld	h,0
	add	hl,de
	ld	a,(hl)
	cp	47
	jp	nz,i_61	;
	ld	hl,0	;const
	add	hl,sp
	ex	de,hl
	ld	hl,1	;const
	add	hl,sp
	ld	a,(hl)
	ld	(de),a
.i_61
	ld	hl,1	;const
	add	hl,sp
	inc	(hl)
	jp	i_59	;EOS
.i_60
	pop	hl
	push	hl
	ld	l,h
	ld	h,0
	ld	a,l
	and	a
	jp	nz,i_62	;
	pop	bc
	ret


.i_62
	ld	hl,4	;const
	add	hl,sp
	ld	e,(hl)
	inc	hl
	ld	d,(hl)
	pop	hl
	push	hl
	ld	h,0
	add	hl,de
	ld	(hl),0
	ld	l,(hl)
	ld	h,0
	pop	bc
	ret


	C_LINE	252,"software/cpm/midiplayer/midiplayer.c::dir_up::0::23"

; Function show_page flags 0x00000200 __smallc 
; unsigned char uint8_tshow_page(const char * dir, unsigned int page)
; parameter 'unsigned int page' at sp+2 size(2)
; parameter 'const char * dir' at sp+4 size(2)
	C_LINE	252,"software/cpm/midiplayer/midiplayer.c::show_page::0::23"
._show_page
	ld	hl,65408	;const
	add	hl,sp
	ld	sp,hl
	ld	hl,130	;const
	add	hl,sp
	call	l_gint	;
	ld	b,h
	ld	c,l
	add	hl,hl
	add	hl,hl
	add	hl,bc
	add	hl,hl
	add	hl,hl
	push	hl
	ld	hl,0
	push	hl
	ld	hl,136	;const
	call	l_gintspsp	;
	ld	hl,136	;const
	add	hl,sp
	call	l_gint	;
	ld	b,h
	ld	c,l
	add	hl,hl
	add	hl,hl
	add	hl,bc
	add	hl,hl
	add	hl,hl
	push	hl
	call	_dir_start
	pop	bc
	pop	bc
	call	l_lneg
	jp	nc,i_63	;
	ld	hl,132	;const
	add	hl,sp
	ld	sp,hl
	ld	hl,0	;const
	ret


.i_63
	pop	hl
	push	hl
	ld	l,h
	ld	a,l
	sub	20
	jp	nc,i_66	;
	ld	hl,4	;const
	add	hl,sp
	push	hl
	ld	hl,128	;const
	push	hl
	ld	l,4
	add	hl,sp
	push	hl
	call	_dir_next
	pop	bc
	pop	bc
	pop	bc
	ld	a,h
	or	l
	jp	z,i_66	;
.i_67_i_66
	pop	bc
	pop	hl
	push	hl
	push	bc
	inc	hl
	push	hl
	call	_con_num
	ld	hl,i_1+7
	ex	(sp),hl
	call	_con_puts
	pop	bc
	ld	hl,4	;const
	add	hl,sp
	push	hl
	call	_con_puts
	pop	bc
	ld	hl,0	;const
	add	hl,sp
	ld	a,(hl)
	and	a
	jp	z,i_68	;
	ld	hl,47	;const
	push	hl
	call	_con_put
	pop	bc
.i_68
	ld	hl,i_1+4
	push	hl
	call	_con_puts
	pop	bc
	pop	de
	pop	hl
	inc	hl
	push	hl
	push	de
	ld	hl,1	;const
	add	hl,sp
	inc	(hl)
	jp	i_64	;EOS
.i_65
	pop	hl
	push	hl
	ld	l,h
	ld	h,0
	exx
	ld	hl,132	;const
	add	hl,sp
	ld	sp,hl
	exx
	ret


	C_LINE	273,"software/cpm/midiplayer/midiplayer.c::show_page::0::25"

; Function nth_entry flags 0x00000200 __smallc 
; unsigned char uint8_tnth_entry(const char * dir, unsigned int n, char * out, unsigned char max, unsigned char uint8_t* is_dir)
; parameter 'unsigned char uint8_t* is_dir' at sp+2 size(2)
; parameter 'unsigned char max' at sp+4 size(1)
; parameter 'char * out' at sp+6 size(2)
; parameter 'unsigned int n' at sp+8 size(2)
; parameter 'const char * dir' at sp+10 size(2)
	C_LINE	274,"software/cpm/midiplayer/midiplayer.c::nth_entry::0::25"
._nth_entry
	call	l_gint8sp	;
	ld	a,h
	or	l
	jp	z,i_70	;
	ld	hl,10	;const
	call	l_gintspsp	;
	ld	hl,10	;const
	add	hl,sp
	call	l_gint	;
	dec	hl
	push	hl
	call	_dir_start
	pop	bc
	pop	bc
	call	l_lneg
	jp	nc,i_69	;
.i_70
	ld	hl,0	;const
	ret


.i_69
	ld	hl,6	;const
	call	l_gintspsp	;
	ld	hl,6	;const
	add	hl,sp
	ld	l,(hl)
	ld	h,0
	push	hl
	call	l_gint6sp	;
	push	hl
	call	_dir_next
	pop	bc
	pop	bc
	pop	bc
	ld	a,l
	and	a
	ld	hl,0	;const
	jr	z,ASMPC+3
	inc	hl
	ret


	C_LINE	280,"software/cpm/midiplayer/midiplayer.c::nth_entry::0::26"

; Function dir_has_entries flags 0x00000200 __smallc 
; unsigned char uint8_tdir_has_entries(const char * dir)
; parameter 'const char * dir' at sp+2 size(2)
	C_LINE	280,"software/cpm/midiplayer/midiplayer.c::dir_has_entries::0::26"
._dir_has_entries
	ld	hl,65407	;const
	add	hl,sp
	ld	sp,hl
	ld	hl,131	;const
	call	l_gintspsp	;
	ld	hl,0	;const
	push	hl
	call	_dir_start
	pop	bc
	pop	bc
	ld	a,h
	or	l
	jp	z,i_72	;
	ld	hl,1	;const
	add	hl,sp
	push	hl
	ld	hl,128	;const
	push	hl
	ld	l,4
	add	hl,sp
	push	hl
	call	_dir_next
	pop	bc
	pop	bc
	pop	bc
	ld	a,l
	and	a
	jp	z,i_72	;
	ld	hl,1	;const
	jr	i_73
.i_72
	ld	hl,0	;const
.i_73
	ld	h,0
	exx
	ld	hl,129	;const
	add	hl,sp
	ld	sp,hl
	exx
	ret


	C_LINE	287,"software/cpm/midiplayer/midiplayer.c::dir_has_entries::0::27"

; Function dir_count flags 0x00000200 __smallc 
; unsigned int uint16_tdir_count(const char * dir)
; parameter 'const char * dir' at sp+2 size(2)
	C_LINE	287,"software/cpm/midiplayer/midiplayer.c::dir_count::0::27"
._dir_count
	ld	hl,65408	;const
	add	hl,sp
	ld	sp,hl
	ld	hl,0	;const
	push	hl
	dec	sp
	ld	hl,133	;const
	call	l_gintspsp	;
	ld	hl,0	;const
	push	hl
	call	_dir_start
	pop	bc
	pop	bc
	call	l_lneg
	jp	nc,i_74	;
	ld	hl,131	;const
	add	hl,sp
	ld	sp,hl
	ld	hl,0	;const
	ret


.i_74
	ld	hl,3	;const
	add	hl,sp
	push	hl
	ld	hl,128	;const
	push	hl
	ld	l,4
	add	hl,sp
	push	hl
	call	_dir_next
	pop	bc
	pop	bc
	pop	bc
	ld	a,h
	or	l
	jp	z,i_76	;
	ld	hl,1	;const
	add	hl,sp
	inc	(hl)
	ld	a,(hl)
	inc	hl
	jr	nz,ASMPC+3
	inc	(hl)
	jp	i_75	;EOS
.i_76
	call	l_gint1sp	;
	exx
	ld	hl,131	;const
	add	hl,sp
	ld	sp,hl
	exx
	ret


	C_LINE	302,"software/cpm/midiplayer/midiplayer.c::dir_count::0::28"
	SECTION	data_compiler
._g_rng
	defw	44257
	SECTION	code_compiler
	C_LINE	304,"software/cpm/midiplayer/midiplayer.c::dir_count::0::28"

; Function rng_next flags 0x00000200 __smallc 
; unsigned int uint16_trng_next()
	C_LINE	304,"software/cpm/midiplayer/midiplayer.c::rng_next::0::28"
._rng_next
	ld	hl,(_g_rng)
	push	hl
	rr	h
	ld	h,l
	rr	h
	ld	l,0
	rr	l
	pop	de
	call	l_xor
	ld	(_g_rng),hl
	push	hl
	ld	hl,(_g_rng)
	ld	de,9
	call	l_asr_u_hl_by_e
	pop	de
	call	l_xor
	ld	(_g_rng),hl
	push	hl
	ld	hl,(_g_rng)
	ld	h,l
	ld	l,0
	pop	de
	call	l_xor
	ld	(_g_rng),hl
	ret


	C_LINE	311,"software/cpm/midiplayer/midiplayer.c::rng_next::0::29"

; Function gcd16 flags 0x00000200 __smallc 
; unsigned int uint16_tgcd16(unsigned int a, unsigned int b)
; parameter 'unsigned int b' at sp+2 size(2)
; parameter 'unsigned int a' at sp+4 size(2)
	C_LINE	311,"software/cpm/midiplayer/midiplayer.c::gcd16::0::29"
._gcd16
.i_78
	pop	bc
	pop	hl
	push	hl
	push	bc
	ld	a,h
	or	l
	jp	z,i_79	;
	ld	hl,4	;const
	call	l_gintspsp	;
	call	l_gint4sp	;
	pop	de
	call	l_div_u
	ex	de,hl
	push	hl
	ld	hl,6	;const
	add	hl,sp
	push	hl
	dec	hl
	dec	hl
	call	l_gint	;
	pop	de
	call	l_pint
	ld	hl,4	;const
	add	hl,sp
	pop	de
	push	de
	ld	(hl),e
	inc	hl
	ld	(hl),d
	ex	de,hl
	pop	bc
	jp	i_78	;EOS
.i_79
	call	l_gint4sp	;
	ret


	C_LINE	322,"software/cpm/midiplayer/midiplayer.c::gcd16::0::31"

; Function pick_stride flags 0x00000200 __smallc 
; unsigned int uint16_tpick_stride(unsigned int count)
; parameter 'unsigned int count' at sp+2 size(2)
	C_LINE	322,"software/cpm/midiplayer/midiplayer.c::pick_stride::0::31"
._pick_stride
	push	bc
	call	l_gint4sp	;
	ld	de,3
	and	a
	sbc	hl,de
	jp	nc,i_80	;
	ld	hl,1	;const
	pop	bc
	ret
.i_81
	call	_rng_next
	push	hl
	call	l_gint6sp	;
	dec	hl
	pop	de
	call	l_div_u
	ex	de,hl
	inc	hl
	pop	bc
	push	hl
	push	hl
	call	l_gint6sp	;
	push	hl
	call	_gcd16
	pop	bc
	pop	bc
	ld	de,1
	and	a
	sbc	hl,de
	jp	nz,i_84	;
	add	hl,sp
	call	l_gint	;
	pop	bc
	ret
.i_82
	pop	bc
	ret


	C_LINE	346,"software/cpm/midiplayer/midiplayer.c::pick_stride::0::33"

; Function browse flags 0x00000200 __smallc 
; unsigned char uint8_tbrowse(char * out)
; parameter 'char * out' at sp+2 size(2)
	C_LINE	346,"software/cpm/midiplayer/midiplayer.c::browse::0::33"
._browse
	ld	hl,65152	;const
	add	hl,sp
	ld	sp,hl
	ld	hl,0	;const
	push	hl
	push	bc
	push	bc
	push	bc
.i_85
	ld	hl,3	;const
	add	hl,sp
	push	hl
	ld	hl,_g_dir
	push	hl
	ld	hl,10	;const
	add	hl,sp
	call	l_gint	;
	push	hl
	call	_show_page
	pop	bc
	pop	bc
	pop	de
	ld	a,l
	ld	(de),a
	ld	hl,3	;const
	add	hl,sp
	ld	a,(hl)
	and	a
	jp	nz,i_89	;
	call	l_gint6sp	;
	ld	a,h
	or	l
	jp	z,i_89	;
.i_90_i_89
	ld	hl,6	;const
	add	hl,sp
	dec	(hl)
	ld	a,(hl)
	inc	hl
	cp	255
	jr	nz,ASMPC+3
	dec	(hl)
	jp	i_85	;EOS
.i_88
	ld	hl,i_1+10
	push	hl
	call	_con_puts
	pop	bc
	ld	a,(_g_dir)
	and	a
	jp	z,i_93	;
	ld	hl,_g_dir
	jp	i_94	;
.i_93
	ld	hl,i_1+15
.i_94
	push	hl
	call	_con_puts
	ld	hl,i_1+17
	ex	(sp),hl
	call	_con_puts
	pop	bc
	call	l_gint6sp	;
	inc	hl
	push	hl
	call	_con_num
	ld	hl,i_1+25
	ex	(sp),hl
	call	_con_puts
	pop	bc
	ld	hl,264	;const
	add	hl,sp
	push	hl
	ld	hl,128	;const
	push	hl
	call	_read_line
	pop	bc
	pop	bc
	ld	hl,264	;const
	add	hl,sp
	ld	a,(hl)
	cp	113
	jp	z,i_96	;
	cp	81
	jp	nz,i_98	;
.i_96
	ld	hl,265	;const
	add	hl,sp
	call	l_gchar
	ld	a,h
	or	l
	jp	nz,i_98	;
	inc	hl
	jr	i_99
.i_98
	ld	hl,0	;const
.i_99
	ld	a,h
	or	l
	jp	z,i_95	;
	ld	hl,392	;const
	add	hl,sp
	ld	sp,hl
	ld	hl,0	;const
	ret


.i_95
	ld	hl,264	;const
	add	hl,sp
	ld	a,(hl)
	cp	46
	jp	nz,i_101	;
	ld	hl,265	;const
	add	hl,sp
	ld	a,(hl)
	cp	46
	jp	nz,i_101	;
	ld	hl,266	;const
	add	hl,sp
	call	l_gchar
	ld	a,h
	or	l
	jp	nz,i_101	;
	inc	hl
	jr	i_102
.i_101
	ld	hl,0	;const
.i_102
	ld	a,h
	or	l
	jp	z,i_100	;
	ld	hl,_g_dir
	push	hl
	call	_dir_up
	pop	bc
	ld	hl,6	;const
	add	hl,sp
	xor	a
	ld	(hl),a
	inc	hl
	ld	(hl),a
	jp	i_85	;EOS
.i_100
	ld	hl,264	;const
	add	hl,sp
	ld	a,(hl)
	cp	110
	jp	z,i_104	;
	cp	78
	jp	nz,i_106	;
.i_104
	ld	hl,265	;const
	add	hl,sp
	call	l_gchar
	ld	a,h
	or	l
	jp	nz,i_106	;
	inc	hl
	jr	i_107
.i_106
	ld	hl,0	;const
.i_107
	ld	a,h
	or	l
	jp	z,i_103	;
	ld	hl,6	;const
	add	hl,sp
	inc	(hl)
	ld	a,(hl)
	inc	hl
	jr	nz,ASMPC+3
	inc	(hl)
	jp	i_85	;EOS
.i_103
	ld	hl,264	;const
	add	hl,sp
	ld	a,(hl)
	cp	112
	jp	z,i_109	;
	cp	80
	jp	nz,i_111	;
.i_109
	ld	hl,265	;const
	add	hl,sp
	call	l_gchar
	ld	a,h
	or	l
	jp	nz,i_111	;
	inc	hl
	jr	i_112
.i_111
	ld	hl,0	;const
.i_112
	ld	a,h
	or	l
	jp	z,i_108	;
	call	l_gint6sp	;
	ld	a,h
	or	l
	jp	z,i_113	;
	ld	hl,6	;const
	add	hl,sp
	dec	(hl)
	ld	a,(hl)
	inc	hl
	cp	255
	jr	nz,ASMPC+3
	dec	(hl)
	jp	i_85	;EOS
.i_108
	ld	hl,265	;const
	add	hl,sp
	ld	a,(hl)
	and	a
	jp	nz,i_115	;
	ld	hl,264	;const
	add	hl,sp
	ld	a,(hl)
	cp	97
	jp	z,i_116	;
	cp	65
	jp	nz,i_115	;
.i_116
	ld	hl,264	;const
	add	hl,sp
	ld	a,(hl)
	cp	97
	jp	nz,i_119	;
	ld	hl,2	;const
	jp	i_120	;
.i_119
	ld	hl,3	;const
.i_120
	ld	h,0
	exx
	ld	hl,392	;const
	add	hl,sp
	ld	sp,hl
	exx
	ret


.i_114
	ld	hl,265	;const
	add	hl,sp
	ld	a,(hl)
	and	a
	jp	nz,i_122	;
	ld	hl,264	;const
	add	hl,sp
	ld	a,(hl)
	cp	114
	jp	z,i_123	;
	cp	82
	jp	nz,i_122	;
.i_123
	ld	hl,264	;const
	add	hl,sp
	ld	a,(hl)
	cp	114
	jp	nz,i_126	;
	ld	hl,4	;const
	jp	i_127	;
.i_126
	ld	hl,5	;const
.i_127
	ld	h,0
	exx
	ld	hl,392	;const
	add	hl,sp
	ld	sp,hl
	exx
	ret


.i_121
	ld	hl,264	;const
	add	hl,sp
	call	l_gchar
	ld	a,h
	or	l
	jp	z,i_85	;EOS
.i_128
	ld	hl,4	;const
	add	hl,sp
	ld	(hl),1
	inc	hl
	ld	(hl),0
	jp	i_131	;EOS
.i_129
	ld	hl,5	;const
	add	hl,sp
	inc	(hl)
.i_131
	ld	hl,264	;const
	add	hl,sp
	ex	de,hl
	ld	hl,5	;const
	add	hl,sp
	ld	l,(hl)
	ld	h,0
	add	hl,de
	ld	a,(hl)
	and	a
	jp	z,i_130	;
	ld	hl,264	;const
	add	hl,sp
	ex	de,hl
	ld	hl,5	;const
	add	hl,sp
	ld	l,(hl)
	ld	h,0
	add	hl,de
	call	l_gchar
	ld	a,l
	sub	48
	ld	a,h
	rla
	ccf
	rra
	sbc	128
	jp	c,i_133	;
	ld	hl,264	;const
	add	hl,sp
	ex	de,hl
	ld	hl,5	;const
	add	hl,sp
	ld	l,(hl)
	ld	h,0
	add	hl,de
	call	l_gchar
	ld	de,57
	ex	de,hl
	call	l_gt
	jp	nc,i_132	;
.i_133
	ld	hl,4	;const
	add	hl,sp
	ld	(hl),0
.i_130
	ld	hl,4	;const
	add	hl,sp
	ld	a,(hl)
	and	a
	jp	z,i_135	;
	ld	hl,0	;const
	pop	bc
	push	hl
	ld	hl,5	;const
	add	hl,sp
	ld	(hl),0
	jp	i_138	;EOS
.i_136
	ld	hl,5	;const
	add	hl,sp
	inc	(hl)
.i_138
	ld	hl,264	;const
	add	hl,sp
	ex	de,hl
	ld	hl,5	;const
	add	hl,sp
	ld	l,(hl)
	ld	h,0
	add	hl,de
	ld	a,(hl)
	and	a
	jp	z,i_137	;
	pop	hl
	push	hl
	ld	b,h
	ld	c,l
	add	hl,hl
	add	hl,hl
	add	hl,bc
	add	hl,hl
	push	hl
	ld	hl,266	;const
	add	hl,sp
	ex	de,hl
	ld	hl,7	;const
	add	hl,sp
	ld	l,(hl)
	ld	h,0
	add	hl,de
	call	l_gchar
	ld	bc,-48
	add	hl,bc
	pop	de
	add	hl,de
	pop	bc
	push	hl
	jp	i_136	;EOS
.i_137
	ld	hl,_g_dir
	push	hl
	ld	hl,2	;const
	call	l_gintspsp	;
	ld	hl,140	;const
	add	hl,sp
	push	hl
	ld	hl,128	;const
	push	hl
	ld	l,10
	add	hl,sp
	push	hl
	call	_nth_entry
	pop	bc
	pop	bc
	pop	bc
	pop	bc
	pop	bc
	call	l_lneg
	jp	nc,i_139	;
	ld	hl,i_1+66
	push	hl
	call	_con_puts
	pop	bc
	jp	i_85	;EOS
.i_139
	ld	hl,2	;const
	add	hl,sp
	ld	a,(hl)
	and	a
	jp	z,i_140	;
	ld	hl,8	;const
	add	hl,sp
	push	hl
	ld	hl,128	;const
	push	hl
	ld	hl,_g_dir
	push	hl
	ld	hl,142	;const
	add	hl,sp
	push	hl
	call	_path_join
	pop	bc
	pop	bc
	pop	bc
	pop	bc
	ld	hl,5	;const
	add	hl,sp
	ld	(hl),0
	jp	i_143	;EOS
.i_141
	ld	hl,5	;const
	add	hl,sp
	inc	(hl)
.i_143
	ld	hl,8	;const
	add	hl,sp
	ex	de,hl
	ld	hl,5	;const
	add	hl,sp
	ld	l,(hl)
	ld	h,0
	add	hl,de
	ld	a,(hl)
	and	a
	jp	z,i_144	;
	ld	hl,5	;const
	add	hl,sp
	ld	a,(hl)
	sub	127
	jp	nc,i_144	;
.i_145_i_144
	ld	de,_g_dir
	ld	hl,5	;const
	add	hl,sp
	ld	l,(hl)
	ld	h,0
	add	hl,de
	push	hl
	ld	hl,10	;const
	add	hl,sp
	ex	de,hl
	ld	hl,7	;const
	add	hl,sp
	ld	l,(hl)
	ld	h,0
	add	hl,de
	call	l_gchar
	pop	de
	ld	a,l
	ld	(de),a
	jp	i_141	;EOS
.i_142
	ld	de,_g_dir
	ld	hl,5	;const
	add	hl,sp
	ld	l,(hl)
	ld	h,0
	add	hl,de
	ld	d,h
	ld	e,l
	xor	a
	ld	(de),a
	ld	hl,6	;const
	add	hl,sp
	xor	a
	ld	(hl),a
	inc	hl
	ld	(hl),a
	jp	i_85	;EOS
.i_140
	ld	hl,5	;const
	add	hl,sp
	ld	(hl),0
	jp	i_148	;EOS
.i_146
	ld	hl,5	;const
	add	hl,sp
	inc	(hl)
.i_148
	ld	hl,136	;const
	add	hl,sp
	ex	de,hl
	ld	hl,5	;const
	add	hl,sp
	ld	l,(hl)
	ld	h,0
	add	hl,de
	ld	a,(hl)
	and	a
	jp	z,i_147	;
	ld	hl,394	;const
	add	hl,sp
	ld	e,(hl)
	inc	hl
	ld	d,(hl)
	ld	hl,5	;const
	add	hl,sp
	ld	l,(hl)
	ld	h,0
	add	hl,de
	push	hl
	ld	hl,138	;const
	add	hl,sp
	ex	de,hl
	ld	hl,7	;const
	add	hl,sp
	ld	l,(hl)
	ld	h,0
	add	hl,de
	call	l_gchar
	pop	de
	ld	a,l
	ld	(de),a
	jp	i_146	;EOS
.i_147
	ld	hl,394	;const
	add	hl,sp
	ld	e,(hl)
	inc	hl
	ld	d,(hl)
	ld	hl,5	;const
	add	hl,sp
	ld	l,(hl)
	ld	h,0
	add	hl,de
	ld	d,h
	ld	e,l
	xor	a
	ld	(de),a
	ld	hl,392	;const
	add	hl,sp
	ld	sp,hl
	ld	hl,1	;const
	ret


.i_135
	ld	hl,5	;const
	add	hl,sp
	ld	(hl),0
	jp	i_151	;EOS
.i_149
	ld	hl,5	;const
	add	hl,sp
	inc	(hl)
.i_151
	ld	hl,264	;const
	add	hl,sp
	ex	de,hl
	ld	hl,5	;const
	add	hl,sp
	ld	l,(hl)
	ld	h,0
	add	hl,de
	ld	a,(hl)
	and	a
	jp	z,i_150	;
	ld	hl,394	;const
	add	hl,sp
	ld	e,(hl)
	inc	hl
	ld	d,(hl)
	ld	hl,5	;const
	add	hl,sp
	ld	l,(hl)
	ld	h,0
	add	hl,de
	push	hl
	ld	hl,266	;const
	add	hl,sp
	ex	de,hl
	ld	hl,7	;const
	add	hl,sp
	ld	l,(hl)
	ld	h,0
	add	hl,de
	call	l_gchar
	pop	de
	ld	a,l
	ld	(de),a
	jp	i_149	;EOS
.i_150
	ld	hl,394	;const
	add	hl,sp
	ld	e,(hl)
	inc	hl
	ld	d,(hl)
	ld	hl,5	;const
	add	hl,sp
	ld	l,(hl)
	ld	h,0
	add	hl,de
	ld	d,h
	ld	e,l
	xor	a
	ld	(de),a
	ld	hl,392	;const
	add	hl,sp
	ld	sp,hl
	ld	hl,1	;const
	ret
.i_86
	ld	hl,392	;const
	add	hl,sp
	ld	sp,hl
	ret


	C_LINE	427,"software/cpm/midiplayer/midiplayer.c::browse::0::43"

; Function pick_start_dir flags 0x00000200 __smallc 
; unsigned char uint8_tpick_start_dir()
	C_LINE	427,"software/cpm/midiplayer/midiplayer.c::pick_start_dir::0::43"
._pick_start_dir
	ld	hl,_g_dir
	ld	d,h
	ld	e,l
	xor	a
	ld	(de),a
	ld	hl,i_1+83
	push	hl
	call	_dir_has_entries
	pop	bc
	ld	a,h
	or	l
	jp	z,i_152	;
	dec	sp
	pop	hl
	ld	l,0
	push	hl
	ld	hl,i_1+83
	push	hl
.i_153
	pop	de
	push	de
	ld	hl,2	;const
	add	hl,sp
	ld	l,(hl)
	ld	h,0
	add	hl,de
	ld	a,(hl)
	and	a
	jp	z,i_155	;
	ld	hl,2	;const
	add	hl,sp
	ld	a,(hl)
	sub	127
	jp	nc,i_155	;
.i_156_i_155
	ld	de,_g_dir
	ld	hl,2	;const
	add	hl,sp
	ld	l,(hl)
	ld	h,0
	add	hl,de
	push	hl
	ld	hl,2	;const
	add	hl,sp
	ld	e,(hl)
	inc	hl
	ld	d,(hl)
	ld	hl,4	;const
	add	hl,sp
	ld	l,(hl)
	ld	h,0
	add	hl,de
	pop	de
	ld	a,(hl)
	ld	(de),a
	ld	hl,2	;const
	add	hl,sp
	inc	(hl)
	jp	i_153	;EOS
.i_154
	ld	de,_g_dir
	ld	hl,2	;const
	add	hl,sp
	ld	l,(hl)
	ld	h,0
	add	hl,de
	ld	d,h
	ld	e,l
	xor	a
	ld	(de),a
	ld	hl,1	;const
	inc	sp
	pop	bc
	ret


.i_152
	ld	hl,i_1+3
	push	hl
	call	_dir_has_entries
	pop	bc
	ld	h,0
	ret


	C_LINE	449,"software/cpm/midiplayer/midiplayer.c::pick_start_dir::0::46"

; Function play_file flags 0x00000200 __smallc 
; unsigned char uint8_tplay_file(const char * path)
; parameter 'const char * path' at sp+2 size(2)
	C_LINE	449,"software/cpm/midiplayer/midiplayer.c::play_file::0::46"
._play_file
	push	bc
	dec	sp
	ld	hl,i_1+89
	push	hl
	call	_con_puts
	pop	bc
	call	l_gint5sp	;
	push	hl
	call	_con_puts
	ld	hl,i_1+98
	ex	(sp),hl
	call	_con_puts
	pop	bc
	ld	hl,0	;const
	ld	a,l
	out	(_FILE_RST),a
	ld	hl,1	;const
	add	hl,sp
	ex	de,hl
	call	l_gint5sp
	call	l_pint
	jp	i_159	;EOS
.i_157
	ld	hl,1	;const
	add	hl,sp
	inc	(hl)
	ld	a,(hl)
	inc	hl
	jr	nz,ASMPC+3
	inc	(hl)
.i_159
	call	l_gint1sp	;
	ld	a,(hl)
	and	a
	jp	z,i_158	;
	call	l_gint1sp	;
	ld	l,(hl)
	ld	h,0
	ld	a,l
	out	(_FILE_NAME),a
	jp	i_157	;EOS
.i_158
	ld	hl,8	;const
	ld	a,l
	out	(_FILE_CMD),a
	ld	hl,0	;const
	add	hl,sp
	push	hl
	in	a,(_FILE_CMD)
	ld	l,a
	pop	de
	ld	(de),a
	pop	hl
	push	hl
	ld	h,0
	ld	a,8
	and	l
	ld	l,a
	jr	nz,ASMPC+3
	scf
	jp	nc,i_160	;
	ld	hl,i_1+104
	push	hl
	call	_con_puts
	pop	bc
	pop	hl
	push	hl
	ld	h,0
	push	hl
	call	_con_hex8
	ld	hl,i_1+122
	ex	(sp),hl
	call	_con_puts
	pop	bc
	ld	hl,2	;const
	ld	a,l
	out	(_FILE_CMD),a
	ld	hl,2	;const
	inc	sp
	pop	bc
	ret


.i_160
	ld	hl,i_1+128
	push	hl
	call	_con_puts
	pop	bc
	call	_midi_play
	ld	hl,2	;const
	ld	a,l
	out	(_FILE_CMD),a
	ld	a,(_midi_stopped)
	and	a
	jp	z,i_161	;
	ld	hl,i_1+164
	push	hl
	call	_con_puts
	pop	bc
	ld	hl,0	;const
	inc	sp
	pop	bc
	ret


.i_161
	ld	hl,1	;const
	inc	sp
	pop	bc
	ret


	C_LINE	479,"software/cpm/midiplayer/midiplayer.c::play_file::0::49"
	C_LINE	487,"software/cpm/midiplayer/midiplayer.c::play_file::0::49"

; Function play_tree flags 0x00000200 __smallc 
; unsigned char uint8_tplay_tree(const char * dir, unsigned char random_order)
; parameter 'unsigned char random_order' at sp+2 size(1)
; parameter 'const char * dir' at sp+4 size(2)
	C_LINE	487,"software/cpm/midiplayer/midiplayer.c::play_tree::0::49"
._play_tree
	ld	hl,65264	;const
	add	hl,sp
	ld	sp,hl
	ld	hl,276	;const
	add	hl,sp
	call	l_gint	;
	push	hl
	call	_dir_count
	pop	bc
	push	hl
	push	bc
	push	bc
	ld	hl,0	;const
	ld	d,h
	ld	e,l
	push	de
	push	hl
	ld	hl,1	;const
	ld	de,0
	push	de
	push	hl
	dec	sp
	ld	hl,13	;const
	add	hl,sp
	call	l_gint	;
	ld	a,h
	or	l
	jp	nz,i_162	;
	exx
	ld	hl,287	;const
	add	hl,sp
	ld	sp,hl
	exx
	ret


.i_162
	ld	hl,289	;const
	add	hl,sp
	ld	a,(hl)
	and	a
	jp	z,i_163	;
	ld	de,(_g_rng)
	ld	hl,(_g_wait_ticks)
	call	l_xor
	ex	de,hl
	ld	hl,13	;const
	add	hl,sp
	call	l_gint	;
	call	l_xor
	ld	de,40503
	call	l_xor
	ld	(_g_rng),hl
	ld	a,h
	or	l
	jp	nz,i_164	;
	inc	hl
	ld	(_g_rng),hl
.i_164
	ld	hl,1	;const
	add	hl,sp
	push	hl
	ld	hl,15	;const
	add	hl,sp
	call	l_gint	;
	push	hl
	call	_pick_stride
	pop	bc
	ld	de,0
	pop	bc
	call	l_plong
	ld	hl,5	;const
	add	hl,sp
	push	hl
	call	_rng_next
	ex	de,hl
	ld	hl,15	;const
	add	hl,sp
	call	l_gint	;
	call	l_div_u
	ex	de,hl
	ld	de,0
	pop	bc
	call	l_plong
.i_163
	ld	hl,11	;const
	add	hl,sp
	xor	a
	ld	(hl),a
	inc	hl
	ld	(hl),a
	jp	i_167	;EOS
.i_165
	ld	hl,11	;const
	add	hl,sp
	inc	(hl)
	ld	a,(hl)
	inc	hl
	jr	nz,ASMPC+3
	inc	(hl)
.i_167
	ld	hl,13	;const
	add	hl,sp
	ld	e,(hl)
	inc	hl
	ld	d,(hl)
	ld	hl,11	;const
	add	hl,sp
	ld	a,(hl)
	inc	hl
	ld	h,(hl)
	ld	l,a
	and	a
	sbc	hl,de
	jp	nc,i_166	;
	ld	hl,9	;const
	add	hl,sp
	push	hl
	ld	hl,291	;const
	add	hl,sp
	ld	a,(hl)
	and	a
	jp	z,i_168	;
	ld	hl,7	;const
	add	hl,sp
	call	l_glong2sp
	ld	hl,17	;const
	add	hl,sp
	call	l_gint	;
	ld	de,0
	push	de
	push	hl
	ld	hl,11	;const
	add	hl,sp
	call	l_glong
	call	l_long_mult_u
	call	l_long_add
	push	de
	push	hl
	ld	hl,19	;const
	add	hl,sp
	call	l_gint	;
	ld	de,0
	call	l_long_mod_u
	jp	i_169	;
.i_168
	ld	hl,13	;const
	add	hl,sp
	call	l_gint	;
.i_169
	pop	de
	call	l_pint
	ld	hl,291	;const
	call	l_gintspsp	;
	ld	hl,11	;const
	add	hl,sp
	call	l_gint	;
	inc	hl
	push	hl
	ld	hl,163	;const
	add	hl,sp
	push	hl
	ld	hl,128	;const
	push	hl
	ld	l,8
	add	hl,sp
	push	hl
	call	_nth_entry
	pop	bc
	pop	bc
	pop	bc
	pop	bc
	pop	bc
	call	l_lneg
	jp	c,i_165	;EOS
.i_170
	ld	hl,15	;const
	add	hl,sp
	push	hl
	ld	hl,144	;const
	push	hl
	ld	hl,295	;const
	call	l_gintspsp	;
	ld	hl,165	;const
	add	hl,sp
	push	hl
	call	_path_join
	pop	bc
	pop	bc
	pop	bc
	pop	bc
	ld	hl,0	;const
	add	hl,sp
	ld	a,(hl)
	and	a
	jp	z,i_171	;
	ld	hl,15	;const
	add	hl,sp
	push	hl
	ld	hl,291	;const
	add	hl,sp
	ld	l,(hl)
	ld	h,0
	push	hl
	call	_play_tree
	pop	bc
	pop	bc
	ld	a,h
	or	l
	jp	z,i_172	;
	ld	hl,287	;const
	add	hl,sp
	ld	sp,hl
	ld	hl,1	;const
	ret
.i_171
	ld	hl,(_g_played)
	inc	hl
	ld	(_g_played),hl
	ld	hl,15	;const
	add	hl,sp
	push	hl
	call	_play_file
	pop	bc
	ld	a,l
	and	a
	jp	nz,i_174	;
	ld	hl,287	;const
	add	hl,sp
	ld	sp,hl
	ld	hl,1	;const
	ret
.i_166
	ld	hl,287	;const
	add	hl,sp
	ld	sp,hl
	ld	hl,0	;const
	ret


	C_LINE	526,"software/cpm/midiplayer/midiplayer.c::play_tree::0::54"

; Function play_dir flags 0x00000200 __smallc 
; void play_dir(unsigned char random_order, unsigned char repeat)
; parameter 'unsigned char repeat' at sp+2 size(1)
; parameter 'unsigned char random_order' at sp+4 size(1)
	C_LINE	526,"software/cpm/midiplayer/midiplayer.c::play_dir::0::54"
._play_dir
	ld	hl,_g_dir
	push	hl
	call	_dir_count
	pop	bc
	ld	a,h
	or	l
	jp	nz,i_175	;
	ld	hl,i_1+179
	push	hl
	call	_con_puts
	pop	bc
	ret


.i_175
	ld	hl,0	;const
	ld	(_g_played),hl
.i_178
	ld	hl,_g_dir
	push	hl
	ld	hl,6	;const
	add	hl,sp
	ld	l,(hl)
	ld	h,0
	push	hl
	call	_play_tree
	pop	bc
	pop	bc
	ld	a,h
	or	l
	jp	nz,i_177	;EOS
.i_179
	ld	hl,(_g_played)
	ld	a,h
	or	l
	jp	z,i_177	;EOS
.i_180
	ld	hl,2	;const
	add	hl,sp
	ld	l,(hl)
	ld	h,0
	ld	a,h
	or	l
	jp	nz,i_178	;EOS
.i_177
	ld	hl,(_g_played)
	ld	a,h
	or	l
	ret	nz
	ld	hl,i_1+200
	push	hl
	call	_con_puts
	pop	bc
.i_181
	ret


	C_LINE	546,"software/cpm/midiplayer/midiplayer.c::play_dir::0::57"
	C_LINE	547,"software/cpm/midiplayer/midiplayer.c::play_dir::0::57"
	C_LINE	549,"software/cpm/midiplayer/midiplayer.c::play_dir::0::57"

; Function parse_tail flags 0x00000200 __smallc 
; void parse_tail()
	C_LINE	549,"software/cpm/midiplayer/midiplayer.c::parse_tail::0::57"
._parse_tail
	ld	hl,128	;const
	push	hl
	ld	l,0
	add	hl,sp
	call	l_gint	;
	ld	l,(hl)
	ld	h,0
	dec	sp
	ld	a,l
	pop	hl
	ld	l,a
	push	hl
	dec	sp
	dec	sp
	pop	hl
	ld	l,0
	push	hl
	ld	hl,2	;const
	add	hl,sp
	ld	e,(hl)
	ld	d,0
	ld	hl,127	;const
	and	a
	sbc	hl,de
	jp	nc,i_182	;
	ld	hl,2	;const
	add	hl,sp
	ld	(hl),127
.i_182
	ld	hl,1	;const
	add	hl,sp
	ld	(hl),0
	jp	i_185	;EOS
.i_183
	ld	hl,1	;const
	add	hl,sp
	inc	(hl)
.i_185
	pop	hl
	push	hl
	ld	e,h
	ld	d,0
	ld	hl,2	;const
	add	hl,sp
	ld	l,(hl)
	ld	h,0
	ex	de,hl
	and	a
	sbc	hl,de
	jp	nc,i_184	;
	ld	hl,3	;const
	call	l_gintspsp	;
	ld	hl,3	;const
	add	hl,sp
	ld	l,(hl)
	ld	h,0
	inc	hl
	pop	de
	add	hl,de
	ld	l,(hl)
	ld	a,l
	rla
	sbc	a
	ld	h,a
	ld	a,l
	call	l_sxt
	dec	sp
	ld	a,l
	pop	hl
	ld	l,a
	push	hl
	ld	a,h
	and	a
	jp	nz,i_187	;
	ld	hl,0	;const
	add	hl,sp
	ld	a,(hl)
	cp	32
	jp	z,i_188	;
	cp	9
	jp	z,i_188	;
	ld	hl,0	;const
	add	hl,sp
	ld	a,(hl)
	and	a
	jp	nz,i_187	;
.i_188
	inc	sp
	jp	i_183	;EOS
.i_186
	ld	hl,0	;const
	add	hl,sp
	ld	a,(hl)
	cp	32
	jp	z,i_192	;
	cp	9
	jp	z,i_192	;
	ld	hl,0	;const
	add	hl,sp
	ld	a,(hl)
	and	a
	jp	z,i_192	;
	ld	hl,0	;const
	add	hl,sp
	ld	a,(hl)
	cp	13
	jp	z,i_192	;
	cp	10
	jp	nz,i_191	;
.i_192
	inc	sp
	jp	i_184	;EOS
.i_191
	ld	hl,_g_arg
	push	hl
	ld	hl,3	;const
	add	hl,sp
	inc	(hl)
	ld	l,(hl)
	ld	h,0
	dec	l
	pop	de
	add	hl,de
	push	hl
	ld	hl,2	;const
	add	hl,sp
	call	l_gchar
	pop	de
	ld	a,l
	ld	(de),a
	inc	sp
	jp	i_183	;EOS
.i_184
	ld	de,_g_arg
	pop	hl
	push	hl
	ld	h,0
	add	hl,de
	ld	d,h
	ld	e,l
	xor	a
	ld	(de),a
	pop	hl
	push	hl
	ld	h,0
	ld	a,l
	and	a
	ld	hl,0	;const
	jr	z,ASMPC+3
	inc	hl
	ld	a,l
	ld	(_g_have_arg),a
	inc	sp
	pop	bc
	pop	bc
	ret


	C_LINE	570,"software/cpm/midiplayer/midiplayer.c::parse_tail::0::59"

; Function main flags 0x00000000 __stdc 
; int main()
	C_LINE	570,"software/cpm/midiplayer/midiplayer.c::main::0::60"
._main
	ld	hl,65263	;const
	add	hl,sp
	ld	sp,hl
	call	_parse_tail
	ld	hl,i_1+220
	push	hl
	call	_con_puts
	pop	bc
	call	_midi_init
	ld	hl,i_1+248
	push	hl
	call	_con_puts
	pop	bc
	in	a,(_MIDI_CFG_BAUD)
	ld	l,a
	ld	h,0
	push	hl
	call	_con_num
	pop	bc
	in	a,(_MIDI_CFG_BAUD)
	ld	e,a
	ld	d,0
	ld	hl,10	;const
	call	l_eq
	jp	nc,i_196	;
	ld	hl,i_1+285
	jp	i_197	;
.i_196
	ld	hl,i_1+289
.i_197
	push	hl
	call	_con_puts
	ld	hl,i_1+123
	ex	(sp),hl
	call	_con_puts
	pop	bc
	ld	a,(_g_have_arg)
	and	a
	jp	z,i_198	;
	ld	hl,_g_arg
	push	hl
	call	_play_file
	pop	bc
	ld	hl,9	;const
	ld	a,l
	out	(_FILE_CMD),a
	call	_midi_restore
	ld	hl,273	;const
	add	hl,sp
	ld	sp,hl
	ld	hl,0	;const
	ret


.i_198
	call	_pick_start_dir
	call	l_lneg
	jp	nc,i_199	;
	ld	hl,i_1+315
	push	hl
	call	_con_puts
	ld	hl,i_1+83
	ex	(sp),hl
	call	_con_puts
	ld	hl,i_1+334
	ex	(sp),hl
	call	_con_puts
	pop	bc
	ld	hl,9	;const
	ld	a,l
	out	(_FILE_CMD),a
	call	_midi_restore
	ld	hl,273	;const
	add	hl,sp
	ld	sp,hl
	ld	hl,1	;const
	ret
.i_200
	ld	hl,0	;const
	add	hl,sp
	push	hl
	ld	hl,147	;const
	add	hl,sp
	push	hl
	call	_browse
	pop	bc
	pop	de
	ld	a,l
	ld	(de),a
	pop	hl
	push	hl
	ld	h,0
	ld	a,l
	and	a
	jp	z,i_201	;EOS
.i_203
	ld	hl,0	;const
	add	hl,sp
	ld	a,(hl)
	cp	1
	jp	nz,i_204	;
	ld	hl,1	;const
	add	hl,sp
	push	hl
	ld	hl,144	;const
	push	hl
	ld	l,149
	add	hl,sp
	push	hl
	call	_make_path
	pop	bc
	pop	bc
	pop	bc
	ld	hl,1	;const
	add	hl,sp
	push	hl
	call	_play_file
	pop	bc
	jp	i_205	;EOS
.i_204
	ld	hl,0	;const
	add	hl,sp
	ld	a,(hl)
	cp	4
	jp	z,i_210	;
	cp	5
	jp	z,i_210	;
	ld	hl,0	;const
	jr	i_211
.i_210
	ld	hl,1	;const
.i_211
	ld	h,0
	push	hl
	ld	hl,2	;const
	add	hl,sp
	ld	a,(hl)
	cp	3
	jp	z,i_212	;
	cp	5
	jp	z,i_212	;
	ld	hl,0	;const
	jr	i_213
.i_212
	ld	hl,1	;const
.i_213
	ld	h,0
	push	hl
	call	_play_dir
	pop	bc
	pop	bc
	jp	i_200	;EOS
.i_201
	ld	hl,9	;const
	ld	a,l
	out	(_FILE_CMD),a
	call	_midi_restore
	ld	hl,i_1+349
	push	hl
	call	_con_puts
	pop	bc
	ld	hl,273	;const
	add	hl,sp
	ld	sp,hl
	ld	hl,0	;const
	ret


	SECTION	bss_compiler
	SECTION	code_compiler
; --- Start of Optimiser additions ---
	defc	i_16 = i_11
	defc	i_21 = i_10
	defc	i_23 = i_22
	defc	i_22 = i_10
	defc	i_32 = i_30
	defc	i_37 = i_36
	defc	i_43 = i_41
	defc	i_49 = i_47
	defc	i_52 = i_51
	defc	i_57 = i_55
	defc	i_66 = i_65
	defc	i_80 = i_83
	defc	i_84 = i_81
	defc	i_89 = i_88
	defc	i_113 = i_85
	defc	i_115 = i_114
	defc	i_122 = i_121
	defc	i_132 = i_129
	defc	i_144 = i_142
	defc	i_155 = i_154
	defc	i_172 = i_173
	defc	i_174 = i_165
	defc	i_187 = i_186
	defc	i_199 = i_202
	defc	i_205 = i_200
	defc	i_5 = i_4
	defc	i_7 = i_6
	defc	i_12 = i_10
	defc	i_13 = i_10
	defc	i_30 = i_29
	defc	i_35 = i_34
	defc	i_45 = i_51
	defc	i_64 = i_63
	defc	i_75 = i_74
	defc	i_83 = i_81
	defc	i_87 = i_85
	defc	i_118_i_115 = i_116
	defc	i_125_i_122 = i_123
	defc	i_173 = i_174
	defc	i_176 = i_180
	defc	i_190_i_187 = i_188
	defc	i_202 = i_200

	SECTION	rodata_compiler
.i_1
	defm	""
	defb	8

	defm	" "
	defb	8

	defm	""
	defb	0

	defm	""
	defb	13

	defm	""
	defb	10

	defm	""
	defb	0

	defm	") "
	defb	0

	defm	"Dir "
	defb	0

	defm	"/"
	defb	0

	defm	"  page "
	defb	0

	defm	"  (num, .., n/p, a/A all, r/R "
	defm	"rand, q): "
	defb	0

	defm	"No such entry."
	defb	13

	defm	""
	defb	10

	defm	""
	defb	0

	defm	"/MIDI"
	defb	0

	defm	"Opening "
	defb	0

	defm	" ... "
	defb	0

	defm	"failed (status 0x"
	defb	0

	defm	")"
	defb	13

	defm	""
	defb	10

	defm	""
	defb	13

	defm	""
	defb	10

	defm	""
	defb	0

	defm	"ok"
	defb	13

	defm	""
	defb	10

	defm	"Playing (q or ESC to stop)..."
	defb	13

	defm	""
	defb	10

	defm	""
	defb	0

	defm	""
	defb	13

	defm	""
	defb	10

	defm	"Stopped."
	defb	13

	defm	""
	defb	10

	defm	""
	defb	13

	defm	""
	defb	10

	defm	""
	defb	0

	defm	"Empty directory."
	defb	13

	defm	""
	defb	10

	defm	""
	defb	13

	defm	""
	defb	10

	defm	""
	defb	0

	defm	"No files found."
	defb	13

	defm	""
	defb	10

	defm	""
	defb	13

	defm	""
	defb	10

	defm	""
	defb	0

	defm	""
	defb	13

	defm	""
	defb	10

	defm	"z80neo CP/M MIDI player"
	defb	13

	defm	""
	defb	10

	defm	""
	defb	0

	defm	"UART1 -> 31250 8N1 (MIDI); cfg"
	defm	" code "
	defb	0

	defm	" ok"
	defb	0

	defm	"  (need firmware code 10)"
	defb	0

	defm	"No files found in "
	defb	0

	defm	" or the root"
	defb	13

	defm	""
	defb	10

	defm	""
	defb	0

	defm	"Bye."
	defb	13

	defm	""
	defb	10

	defm	""
	defb	0


; --- Start of Static Variables ---

	SECTION	bss_compiler
._g_wait_ticks	defs	2
._g_old_baud	defs	1
._g_old_fmt	defs	1
._g_have_cfg	defs	1
._g_dir	defs	128
._g_played	defs	2
._g_arg	defs	128
._g_have_arg	defs	1
	SECTION	code_compiler


; --- Start of Scope Defns ---

	GLOBAL	__CLIB_OPEN_MAX
	GLOBAL	__fcb
	GLOBAL	get_dpb
	GLOBAL	bdos
	GLOBAL	bdos_callee
	GLOBAL	bdosh
	GLOBAL	bdosh_callee
	GLOBAL	bios
	GLOBAL	biosh
	GLOBAL	getfcb
	GLOBAL	cpm_cache_get
	GLOBAL	cpm_cache_flush
	GLOBAL	setfcb
	GLOBAL	parsefcb
	GLOBAL	putoffset
	GLOBAL	_putoffset
	GLOBAL	swapuid
	GLOBAL	_fc_dir
	GLOBAL	_fc_dirpos
	GLOBAL	_fc_dirbuf
	GLOBAL	change_volume
	GLOBAL	get_current_volume
	GLOBAL	dir_move_first
	GLOBAL	dir_move_next
	GLOBAL	dir_get_entry_type
	GLOBAL	dir_get_entry_name
	GLOBAL	dir_get_entry_size
	GLOBAL	get_dir_name
	GLOBAL	a_statusline
	GLOBAL	a_keyspeed
	GLOBAL	a_border
	GLOBAL	a_paper
	GLOBAL	a_ink
	GLOBAL	a_curx
	GLOBAL	a_cury
	GLOBAL	a_machine
	GLOBAL	a_machinever
	GLOBAL	a_biosver
	GLOBAL	a_memsize
	GLOBAL	a_driveb
	GLOBAL	a_serialport
	GLOBAL	_gios_pb
	GLOBAL	_gios_ctl
	GLOBAL	_gios_intin
	GLOBAL	_gios_ptsin
	GLOBAL	_gios_intout
	GLOBAL	_gios_ptsout
	GLOBAL	gios
	GLOBAL	gios_1pm
	GLOBAL	gios_1pm_callee
	GLOBAL	gios_2px
	GLOBAL	gios_2px_callee
	GLOBAL	gios_1px
	GLOBAL	gios_1px_callee
	GLOBAL	gios_text
	GLOBAL	gios_esc
	defc	_MIDI_DATA =	144
	defc	_MIDI_STATUS =	145
	defc	_MIDI_CFG_BAUD =	146
	defc	_MIDI_CFG_FMT =	147
	defc	_FILE_CMD =	160
	defc	_FILE_NAME =	161
	defc	_FILE_DATA =	162
	defc	_FILE_RST =	163
	GLOBAL	_midi_delay_ms
	GLOBAL	_midi_play
	GLOBAL	_midi_stopped
	GLOBAL	_main


; --- End of Scope Defns ---


; --- End of Compilation ---
