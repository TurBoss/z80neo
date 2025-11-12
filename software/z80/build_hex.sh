
if [ $1 ]
then
    z88dk-z80asm -b $1.s
    bin2hex.py $1.bin $1_tmp.hex

    sed 's/^.\{9\}//g' $1_tmp.hex > $1.hex
    rm $1_tmp.hex
    rm $1.o

    #  srec_cat leds.bin -Raw -o leds.hex -VMem 8 
    echo "OK!"
fi
