' EXPECT: anglen= 2|angle0= 0
IMPORT LINALG
SUB Main()
  DIM a(3) AS DOUBLE
  a(0)=2 : a(1)=1 : a(2)=1 : a(3)=2
  DIM b(3) AS DOUBLE
  b(0)=1 : b(1)=2 : b(2)=2 : b(3)=3
  DIM ang(1) AS DOUBLE
  DIM n AS LONG
  n = LINALG.SUBSPACEANGLES(a, 2, 2, b, 2, 2, ang)
  PRINT "anglen="; n
  PRINT "angle0="; MATH.ROUND(ang(0),6)
END SUB
