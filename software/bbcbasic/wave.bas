   10 REM Sine wave - z80neo BBC BASIC
   20 DEF FNp$(x%,y%)=CHR$(27)+"["+FNq$(y%+1)+";"+FNq$(x%+1)+"H"
   30 DEF FNq$(n%)=CHR$(48+n%DIV10)+CHR$(48+n%MOD10)
   40 CLS
   50 COLOUR 3
   60 T=0
   70 REPEAT
   80 CLS
   90 FOR X%=0 TO 79 STEP 2
  100 Y%=11+INT(9*SIN((X%+T)/8))
  110 PRINT FNp$(X%,Y%);CHR$(219);
  120 NEXT
  130 T=T+0.5
  140 UNTIL FALSE
RUN
