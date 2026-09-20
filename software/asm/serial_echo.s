;---- PUERTOS
SERIAL_DATA:	EQU 0x80
SERIAL_STATUS:	EQU 0x81



  ;--- Comienzo del programa
  ORG 0x0000


MAIN:
  ;-- Configurar la pila
  LD	SP,		0x3fff
  ;LD	SP,		topOfStack



;-- Bucle principal
LOOP:

  CALL READ_CHAR
  CALL PRINT_CHAR

  JP LOOP



;-------------------------------------
;-- Subrutina para recibir un caracter
;-- Se queda esperando hasta que llega
;-- SALIDAS:
;--   A: Contiene el carater recibido
;--------------------------------------
READ_CHAR:

  ;-- Esperar hasta que llegue un caracter
  IN A, (SERIAL_STATUS)

  AND 2

  JR NZ, READ_CHAR ;-- No llega, esperar

  ;-- Leer el caracter que ha llegado
  IN A, (SERIAL_DATA)

  ;-- Retorno. A contiene el caracter recibido
  RET



;-----------------------------------------------------------
;-- Subrutina para enviar un caracter por el puerto serie
;-- ENTRADAS:
;--   A: Contiene el caracter a enviar
;-----------------------------------------------------------
PRINT_CHAR:

  ;-- Guardar A en la pila, para no perderlo
  PUSH AF

READY_TX:

  ;-- Leer registro de estaus de la UART
  ;-- ¿Se puede enviar?
  IN A, (SERIAL_STATUS)
  AND 1
  JR Z, READY_TX ;-- No--> Esperar

  ;-- Listo para transmitir

  ;-- Recuperar de la pila el caracter a enviar
  POP AF

  ;-- Enviar caracter
  OUT (SERIAL_DATA), A

  ;-- Retorno
  RET



topOfStack:
