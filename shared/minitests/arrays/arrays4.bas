' EXPECT: fN= 3|f= 3 , 4 , 2
SUB Main()
  DIM a(4)
  a(0) = 3
  a(1) = 1
  a(2) = 4
  a(3) = 1
  a(4) = 2
  DIM f = ARRAYS.AFILTER(a, FN(x) => x > 1)
  PRINT "fN="; UBOUND(f) + 1
  PRINT "f="; f(0); ","; f(1); ","; f(2)
END SUB
