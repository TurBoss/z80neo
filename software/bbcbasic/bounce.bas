   10 REM Bouncing balls - z80neo BBC BASIC
   20 DEF FNp$(x%,y%)=CHR$(27)+"["+FNq$(y%+1)+";"+FNq$(x%+1)+"H"
   30 DEF FNq$(n%)=CHR$(48+n%DIV10)+CHR$(48+n%MOD10)
   40 B%=3
   50 DIM X%(B%),Y%(B%),U%(B%),V%(B%)
   60 FOR I%=0 TO B%
   70 X%(I%)=RND(60)+10
   80 Y%(I%)=RND(15)+3
   90 U%(I%)=RND(3)-2
  100 V%(I%)=RND(3)-2
  110 IF U%(I%)=0 THEN U%(I%)=1
  120 IF V%(I%)=0 THEN V%(I%)=1
  130 NEXT
  140 CLS
  150 REPEAT
  160 FOR I%=0 TO B%
  170 PRINT FNp$(X%(I%),Y%(I%));" ";
  180 X%(I%)=X%(I%)+U%(I%)
  190 Y%(I%)=Y%(I%)+V%(I%)
  200 IF X%(I%)<1 OR X%(I%)>78 THEN U%(I%)=-U%(I%)
  210 IF Y%(I%)<1 OR Y%(I%)>22 THEN V%(I%)=-V%(I%)
  220 NEXT
  230 FOR I%=0 TO B%
  240 PRINT FNp$(X%(I%),Y%(I%));CHR$(219);
  250 NEXT
  260 UNTIL FALSE
RUN
