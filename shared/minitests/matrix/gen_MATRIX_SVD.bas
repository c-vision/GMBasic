' EXPECT: s00= 3
IMPORT LINALG
SUB Main()
  DIM a(3) AS DOUBLE
  a(0)=2 : a(1)=1 : a(2)=1 : a(3)=2
  DIM u(3) AS DOUBLE, s(1) AS DOUBLE, vt(3) AS DOUBLE
  LINALG.SVD(a, 2, 2, u, s, vt)
  PRINT "s00="; MATH.ROUND(s(0),6)
END SUB
