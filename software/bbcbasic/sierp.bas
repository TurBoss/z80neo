   10 REM Sierpinski triangle (Pascal mod 2) - z80neo BBC BASIC
   20 W%=47
   30 DIM R%(W%+1),S%(W%+1)
   40 CLS
   50 FOR K%=0 TO W%
   60 R%(K%)=0
   70 NEXT
   80 R%(0)=1
   90 FOR Y%=0 TO 20
  100 PRINT SPC((W%-Y%) DIV 2);
  110 FOR X%=0 TO Y%
  120 IF R%(X%) THEN PRINT CHR$(219); ELSE PRINT " ";
  130 NEXT
  140 PRINT
  150 S%(0)=1
  160 FOR X%=1 TO Y%+1
  170 S%(X%)=(R%(X%-1)+R%(X%)) MOD 2
  180 NEXT
  190 FOR X%=0 TO Y%+1
  200 R%(X%)=S%(X%)
  210 NEXT
  220 NEXT Y%
  230 PRINT
RUN
