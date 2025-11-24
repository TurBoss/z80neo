SERIAL_DATA:	equ 0x80
SERIAL_STATUS:  equ 0x81

org 0x0000

MAIN:
  ld	sp,		0x3fff

LOOP:
  in A, (SERIAL_STATUS)

  and 0x01                   ; Check data ready bit (bit 0)

  jr z,  LOOP

  in A, (SERIAL_DATA)

  out (SERIAL_DATA), A

  jr LOOP



topOfStack:
