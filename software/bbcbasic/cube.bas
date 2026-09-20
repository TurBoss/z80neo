   10 REM Rotating wireframe cube - VDP bitmap (DVI) demo
   20 MODE 1
   30 DIM VX(7),VY(7),VZ(7),SX(7),SY(7),P(11),Q(11)
   40 VX(0)=-1:VY(0)=-1:VZ(0)=-1
   50 VX(1)=1:VY(1)=-1:VZ(1)=-1
   60 VX(2)=1:VY(2)=1:VZ(2)=-1
   70 VX(3)=-1:VY(3)=1:VZ(3)=-1
   80 VX(4)=-1:VY(4)=-1:VZ(4)=1
   90 VX(5)=1:VY(5)=-1:VZ(5)=1
  100 VX(6)=1:VY(6)=1:VZ(6)=1
  110 VX(7)=-1:VY(7)=1:VZ(7)=1
  120 P(0)=0:Q(0)=1:P(1)=1:Q(1)=2:P(2)=2:Q(2)=3:P(3)=3:Q(3)=0
  130 P(4)=4:Q(4)=5:P(5)=5:Q(5)=6:P(6)=6:Q(6)=7:P(7)=7:Q(7)=4
  140 P(8)=0:Q(8)=4:P(9)=1:Q(9)=5:P(10)=2:Q(10)=6:P(11)=3:Q(11)=7
  150 A=0
  160 REPEAT
  170 CLG
  180 GCOL 3
  190 CA=COS(A):SA=SIN(A):CB=COS(A*0.73):SB=SIN(A*0.73)
  200 FOR I=0 TO 7
  210 X=VX(I):Y=VY(I):Z=VZ(I)
  220 Y1=Y*CA-Z*SA
  230 Z1=Y*SA+Z*CA
  240 X2=X*CB+Z1*SB
  250 SX(I)=320+X2*170
  260 SY(I)=128-Y1*170
  270 NEXT
  280 FOR I=0 TO 11
  290 MOVE SX(P(I)),SY(P(I))
  300 DRAW SX(Q(I)),SY(Q(I))
  310 NEXT
  320 A=A+0.2
  330 UNTIL FALSE
RUN
