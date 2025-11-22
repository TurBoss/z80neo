;port
SERIAL:	equ	0x80

org 0x0000

main:
  ld sp, 0x3fff


loop:
  in A, (SERIAL)

  out (SERIAL), A

  jp loop


topOfStack:
