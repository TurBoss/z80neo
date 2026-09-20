   10 REM Matrix rain - z80neo BBC BASIC
   20 DEF FNp$(x%,y%)=CHR$(27)+"["+FNq$(y%+1)+";"+FNq$(x%+1)+"H"
   30 DEF FNq$(n%)=CHR$(48+n%DIV10)+CHR$(48+n%MOD10)
   40 A$="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789@#$%&*+-<>"
   50 L%=LEN(A$)
   60 N%=79
   70 DIM Y%(N%)
   80 FOR I%=0 TO N%
   90 Y%(I%)=RND(24)-1
  100 NEXT
  110 CLS
  120 COLOUR 2
  130 REPEAT
  140 FOR K%=1 TO 12
  150 I%=RND(80)-1
  160 T%=Y%(I%)-6
  170 IF T%<0 THEN T%=T%+24
  180 PRINT FNp$(I%,T%);" ";
  190 Y%(I%)=Y%(I%)+1
  200 IF Y%(I%)>23 THEN Y%(I%)=0
  210 PRINT FNp$(I%,Y%(I%));MID$(A$,RND(L%),1);
  220 NEXT
  230 UNTIL FALSE
RUN
