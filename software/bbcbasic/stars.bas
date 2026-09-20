   10 REM Starfield - z80neo BBC BASIC
   20 DEF FNp$(x%,y%)=CHR$(27)+"["+FNq$(y%+1)+";"+FNq$(x%+1)+"H"
   30 DEF FNq$(n%)=CHR$(48+n%DIV10)+CHR$(48+n%MOD10)
   40 N%=24
   50 DIM X%(N%),Y%(N%),V%(N%)
   60 FOR I%=0 TO N%
   70 X%(I%)=RND(80)-1
   80 Y%(I%)=RND(24)-1
   90 V%(I%)=RND(3)
  100 NEXT
  110 COLOUR 7
  120 REPEAT
  130 CLS
  140 FOR I%=0 TO N%
  150 X%(I%)=X%(I%)-V%(I%)
  160 IF X%(I%)<0 THEN X%(I%)=79
  170 Y%(I%)=Y%(I%)+1
  180 IF Y%(I%)>23 THEN Y%(I%)=0
  190 PRINT FNp$(X%(I%),Y%(I%));CHR$(175+V%(I%));
  200 NEXT
  210 UNTIL FALSE
RUN
