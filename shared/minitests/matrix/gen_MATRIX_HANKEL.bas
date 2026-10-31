' EXPECT: hk= 4
IMPORT MATRIX
IMPORT MATH
SUB Main()
  DIM c(2), r(2)
  c(0)=1 : c(1)=2 : c(2)=3
  r(0)=3 : r(1)=4 : r(2)=5
  DIM hk = MATRIX.HANKEL(c, r)
  PRINT "hk="; MATH.ROUND(MATRIX.GETVALUE(hk,2,1),6)
END SUB