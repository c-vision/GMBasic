' EXPECT: sum= 11
SUB Main()
  DIM a(4)
  a(0) = 3
  a(1) = 1
  a(2) = 4
  a(3) = 1
  a(4) = 2
  DIM r = ARRAYS.AREDUCE(a, FN(acc, x) => acc + x)
  PRINT "sum="; r
END SUB
