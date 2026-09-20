SERIAL_DATA:	equ 0x80
SERIAL_STATUS:  equ 0x81

org 0x0000

MAIN:
  ld	sp,		topOfStack

LOOP:

  in A, (SERIAL_STATUS)

  and 0x04                   ; Check data ready bit (bit 2)

  jr z, LOOP

  in A, (SERIAL_DATA)

  out (SERIAL_DATA), A

  jr LOOP



topOfStack:
