' EXPECT: ev= 3
IMPORT LINALG
SUB Main()
  DIM a(3) AS DOUBLE
  a(0)=2 : a(1)=1 : a(2)=1 : a(3)=2
  DIM d(1) AS DOUBLE, v(3) AS DOUBLE
  LINALG.EVD(a, 2, d, v)
  PRINT "ev="; MATH.ROUND(d(1),6)
END SUB
