' EXPECT: p00=-24
IMPORT LINALG
SUB Main()
  DIM a(8) AS DOUBLE
  a(0)=1 : a(1)=2 : a(2)=3
  a(3)=0 : a(4)=1 : a(5)=4
  a(6)=5 : a(7)=6 : a(8)=0
  DIM p(8) AS DOUBLE
  LINALG.PINVERSE(a, 3, 3, p)
  PRINT "p00="; MATH.ROUND(p(0), 6)
END SUB
