;-- UART. Ejemplo 04: Prueba de lectura del canal de datos recibidos de
;-- la UART. Se esta constantemente leyendo su valor y sacandolo por los
;-- LEDs. Si desde el terminal enviamos caracteres, veremos su valor
;-- ASCII en los LEDs

;---- PUERTOS
LEDS:			equ	40H
SERIAL_DATA:	equ	80H

;--- Comienzo del programa
org 0x0000

main:
  ;-- Configurar la pila
  ld sp, 0x3fff


loop:
  ;-- Leer lo ultimo recibido por la UART
  in A, (SERIAL_DATA)

  ;-- Mostrarlo en los LEDs
  out (LEDS), A

  ;-- Repetir
  jp loop



;  org 0x3fff
;topOfStack:
