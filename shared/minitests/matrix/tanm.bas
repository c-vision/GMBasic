' EXPECT: tanm= 0.707431
IMPORT LINALG
SUB Main()
  DIM a(3) AS DOUBLE
  a(0)=2 : a(1)=1 : a(2)=1 : a(3)=2
  DIM r(3) AS DOUBLE
  LINALG.TANM(a, 2, r)
  PRINT "tanm="; MATH.ROUND(ABS(r(0)), 6)
END SUB
