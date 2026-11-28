' EXPECT: u00= 3
IMPORT LINALG
SUB Main()
  DIM a(3) AS DOUBLE
  a(0)=1 : a(1)=2 : a(2)=3 : a(3)=4
  DIM l(3) AS DOUBLE, u(3) AS DOUBLE, p(3) AS DOUBLE
  LINALG.LU(a, 2, l, u, p)
  PRINT "u00="; MATH.ROUND(u(0),6)
END SUB
