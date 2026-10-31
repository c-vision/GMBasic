' EXPECT: max= 4|min= 1|sum= 10
SUB Main()
  DIM data(3)
  data(0) = 1
  data(1) = 2
  data(2) = 3
  data(3) = 4
  PRINT "max="; ARRAYS.AMAX(data)
  PRINT "min="; ARRAYS.AMIN(data)
  PRINT "sum="; ARRAYS.ASUM(data)
END SUB
