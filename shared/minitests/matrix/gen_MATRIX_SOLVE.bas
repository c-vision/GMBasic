' EXPECT: x0= 1
IMPORT LINALG
SUB Main()
  DIM a(3) AS DOUBLE
  a(0)=2 : a(1)=1 : a(2)=1 : a(3)=2
  DIM b(1) AS DOUBLE
  b(0)=4 : b(1)=5
  DIM x(1) AS DOUBLE
  LINALG.SOLVE(a, 2, b, x)
  PRINT "x0="; MATH.ROUND(x(0),6)
END SUB
