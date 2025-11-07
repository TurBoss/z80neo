PORT_LEDS: equ 0x40
VAL_A equ 0xBA
VAL_B equ 0xAB

;org 0x0000

main:
;  ld	sp, 0x3fff
  
loop:
  ld    A, VAL_A
  out	(PORT_LEDS), A
    
  ld    A, VAL_B
  out	(PORT_LEDS), A
  
  JP	loop

topOfStack: