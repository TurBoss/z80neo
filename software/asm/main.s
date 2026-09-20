; SECTION CODE

;---- PUERTOS

SERIAL_DATA:    EQU 80h
SERIAL_STATUS:  EQU 81h


;--- Comienzo del programa
ORG 0x0000



MAIN:

    ;-- Configurar la pila
    ; LD SP, 0x7fff
    LD SP, topOfStack    ; Point SP to the TOP of our stack area

    
    
    ;-- Bucle principal

LOOP:


    CALL READ_CHAR
    
    CALL PRINT_CHAR

    JP LOOP
    
    HALT                 ; Just in case

;-------------------------------------
;-- Subrutina para recibir un caracter
;-- Se queda esperando hasta que llega
;-- SALIDAS:
;--   A: Contiene el carater recibido
;--------------------------------------
READ_CHAR:
    ;-- Esperar hasta que llegue un caracter
    IN A, (SERIAL_STATUS)

    AND 0x02
    JR Z, READ_CHAR      ; Z=1 means bit is 0 -> no data, keep waiting
    ;-- Si llegamos aquí, hay datos disponibles (bit 2 = 1)

    ;-- Leer el caracter que ha llegado
    IN A, (SERIAL_DATA)
    RET

;-----------------------------------------------------------
;-- Subrutina para enviar un caracter por el puerto serie
;-- ENTRADAS:
;--   A: Contiene el caracter a enviar
;-----------------------------------------------------------
PRINT_CHAR:
    PUSH AF              ; Save the character

ready_tx:
    ;-- Leer registro de estaus de la UART
    IN A, (SERIAL_STATUS)
    AND 0x01             ; Check if transmitter is ready (bit 0 = 1)
    JR Z, ready_tx       ; If Z=1, bit is 0 -> not ready, keep waiting

    ;-- Recuperar de la pila el caracter a enviar
    POP AF
    
    ;-- Enviar caracter
    OUT (SERIAL_DATA), A
    RET

; =======================================================
; DATA SECTION - Reserve memory for stack and variables
; =======================================================

; SECTION DATA
; ORG 0x7E00

stack_memory:
    defs 256, 0          ; Reserve 256 bytes for stack, initialized to 0

topOfStack:              ; This label points to the TOP of the stack
