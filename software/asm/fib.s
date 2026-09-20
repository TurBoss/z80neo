; 8 BIT VERSION
; IN : A = N  (N <= 13, OTHERWISE OVERFLOWS)
; OUT: A = FIB(N)

	ORG 0x0000

MAIN:
	LD		SP,	topOfStack

FIB8:
    CP		2
	RET 	C		; IF N < 2 THEN DONE

	LD		B,	A
	DEC		B		; B = N - 1
	LD		C,	0	; F0
	LD		A,	1	; F1


F8_L:
	LD		D,	A
	ADD		A,	C
	LD		C,	D
	DJNZ	F8_L

	RET


topOfStack:
