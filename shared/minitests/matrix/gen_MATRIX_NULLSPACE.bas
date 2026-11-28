' EXPECT: ns= 0.707107
IMPORT LINALG
SUB Main()
  DIM a(3) AS DOUBLE
  a(0)=1 : a(1)=1 : a(2)=1 : a(3)=1
  DIM ns(1) AS DOUBLE
  LINALG.NULLSPACE(a, 2, 2, ns)
  PRINT "ns="; MATH.ROUND(ABS(ns(0)),6)
END SUB
