;---- PUERTOS

SERIAL_DATA:   equ 0x80
SERIAL_STATUS: equ 0x81



;--- Comienzo del programa
  ORG 0x0000


main:

  ;-- Configurar la pila
  LD SP, topOfStack

main_loop:

  ;-- HL apunta a la cadena a enviar
  LD HL, MSG

;  jp main_loop

loop:

  ;-- Leer registro de estaus de la UART
  ;-- ¿Se puede enviar?
;  IN A, (SERIAL_STATUS)
;  AND 0x02
;  JR NZ, loop ;-- No--> Esperar

  ;-- Listo para transmitir
  INC HL
  ;-- Leer el primer caracter
  LD A, (HL)

  ;-- Es el ultimo caracter?
  CP 0
  JR NZ, fin  ;-- Si--> Terminar

  ;--- No. Enviar caracter
  OUT (SERIAL_DATA), A

  ;-- Apuntar al siguiente caracter


  ;-- Repetir
  JP loop


fin:
  halt


;--- Cadena a enviar. Deber terminar en 0
MSG: DB "Hellord !!!\r\n", 0


topOfStack:
