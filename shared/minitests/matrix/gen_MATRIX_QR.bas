' EXPECT: r00= 3.162278
IMPORT LINALG
SUB Main()
  DIM a(3) AS DOUBLE
  a(0)=1 : a(1)=2 : a(2)=3 : a(3)=4
  DIM q(3) AS DOUBLE, r(3) AS DOUBLE
  LINALG.QR(a, 2, 2, q, r)
  PRINT "r00="; MATH.ROUND(r(0),6)
END SUB
