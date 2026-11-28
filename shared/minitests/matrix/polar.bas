' EXPECT: v= 1
IMPORT LINALG
SUB Main()
  DIM a(3) AS DOUBLE
  a(0)=2 : a(1)=1 : a(2)=1 : a(3)=2
  DIM u(3) AS DOUBLE, p(3) AS DOUBLE
  LINALG.POLAR(a, 2, u, p)
  PRINT "v="; MATH.ROUND(u(0), 6)
END SUB
