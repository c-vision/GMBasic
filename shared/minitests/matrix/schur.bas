' EXPECT: v= 2
IMPORT LINALG
SUB Main()
  DIM a(3) AS DOUBLE
  a(0)=2 : a(1)=1 : a(2)=1 : a(3)=2
  DIM t(3) AS DOUBLE, z(3) AS DOUBLE
  LINALG.SCHUR(a, 2, t, z)
  PRINT "v="; MATH.ROUND(t(0), 6)
END SUB
