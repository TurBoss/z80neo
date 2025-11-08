z88dk-z80asm -b leds.asm
# bin2hex.py leds.bin leds.hex
srec_cat leds.bin -Raw -o leds.hex -VMem 8 