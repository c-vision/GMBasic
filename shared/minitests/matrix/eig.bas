' EXPECT: v= 3
IMPORT LINALG
SUB Main()
  DIM a(3) AS DOUBLE
  a(0)=2 : a(1)=1 : a(2)=1 : a(3)=2
  DIM wr(1) AS DOUBLE, wi(1) AS DOUBLE, vr(3) AS DOUBLE, vi(3) AS DOUBLE
  LINALG.EIG(a, 2, wr, wi, vr, vi)
  PRINT "v="; MATH.ROUND(wr(0), 6)
END SUB
