;---- PUERTOS
LEDS:			equ 0x40
SERIAL_DATA:	equ 0x80
SERIAL_STATUS:	equ 0x81

;--- Comienzo del programa
org 0x0000

MAIN:
  ;-- Configurar la pila
  ld	sp,		0x3fff

;-- Bucle principal
LOOP:

  ;-- Esperar hasta que se reciba un caracter
  call READ_CHAR

  ;-- Sacar el caracter por los LEDs
  out	(LEDS), A

  ;-- Enviar el caracter la PC (eco)
  call PRINT_CHAR

  ;-- Repetir
  jr LOOP


;-------------------------------------
;-- Subrutina para recibir un caracter
;-- Se queda esperando hasta que llega
;-- SALIDAS:
;--   A: Contiene el carater recibido
;--------------------------------------
READ_CHAR:

  ;-- Esperar hasta que llegue un caracter
  in A, (SERIAL_STATUS)

  and 0x02

  jr z, READ_CHAR ;-- No llega, esperar

  ;-- Leer el caracter que ha llegado
  in A, (SERIAL_DATA)

  ;-- Retornar. A contiene el caracter recibido
  ret


;-----------------------------------------------------------
;-- Subrutina para enviar un caracter por el puerto serie
;-- ENTRADAS:
;--   A: Contiene el caracter a enviar
;-----------------------------------------------------------
PRINT_CHAR:

  ;-- Guardar A en la pila, para no perderlo
  push AF

READY_TX:

  ;-- Leer registro de estaus de la UART
  ;-- ¿Se puede enviar?
  in A, (SERIAL_STATUS)
  and 0x01
  jp nz, READY_TX ;-- No--> Esperar

  ;-- Listo para transmitir

  ;-- Recuperar de la pila el caracter a enviar
  pop AF

  ;-- Enviar caracter
  out (SERIAL_DATA), A

  ;-- Retornar
  ret

topOfStack:
