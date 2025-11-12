z80neo
======


![z80neo logo](https://github.com/turboss/z80neo/blob/main/images/logo_mon.png?raw=true)

```
          ______  _______
________ /  __  \ \   _  \    ____    ____    ____
\___   / >      < /  /_\  \  /    \ _/ __ \  /  _ \
 /    / /   --   \\  \_/   \|   |  \\  ___/ (  <_> )
/_____ \\______  / \_____  /|___|  / \___  > \____/
      \/       \/        \/      \/      \/

```




- Another z80 board

A Raspberry Pi Pico 2 (RP235x) based computer arround the z80 cpu

SD Card Interface

Serial Port Interface



https://github.com/turboss/z80neo



Join live chat


- matrix

	https://matrix.to/#/#z80neo:jauriarts.org

- discord

	todo

- IRC

	todo



Schematics
==========


![z80neo schematics](https://github.com/turboss/z80neo/blob/main/images/z80neo.png?raw=true)


Software
========

- z88dk-asm

	https://github.com/z88dk/z88dk/


```ASM
PORT_LEDS: equ 0x40
VAL_A equ 0xBA
VAL_B equ 0xAB

main:
  ld	sp, 0x3fff
  
loop:
  ld    A, VAL_A
  out	(PORT_LEDS), A
    
  ld    A, VAL_B
  out	(PORT_LEDS), A
  
  JP	loop

topOfStack
```


pictures
========

![z80neo full thing](https://github.com/turboss/z80neo/blob/main/images/full_thing.jpg?raw=true)

![z80neo cpu wires](https://github.com/turboss/z80neo/blob/main/images/cpu_back.jpg?raw=true)

![z80neo pulseview](https://github.com/turboss/z80neo/blob/main/images/pulse_view.png?raw=true)

![z80neo screen](https://github.com/turboss/z80neo/blob/main/images/screen.png?raw=true)

![z80neo serial](https://github.com/turboss/z80neo/blob/main/images/serial_out.png?raw=true)



Referemces
==========


- picoram6116

	https://github.com/lambdamikel/picoram6116

- neo6502 

	https://github.com/OLIMEX/Neo6502