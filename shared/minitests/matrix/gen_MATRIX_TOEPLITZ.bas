' EXPECT: tp= 4
IMPORT MATRIX
IMPORT MATH
SUB Main()
  DIM c(2), r(2)
  c(0)=1 : c(1)=2 : c(2)=3
  r(0)=1 : r(1)=4 : r(2)=5
  DIM tp = MATRIX.TOEPLITZ(c, r)
  PRINT "tp="; MATH.ROUND(MATRIX.GETVALUE(tp,0,1),6)
END SUB