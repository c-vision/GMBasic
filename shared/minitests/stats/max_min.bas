' EXPECT: max= 9|min= 0
SUB Main()
  DIM x() = {4, 1, 7, 2, 9, 0, 5}
  PRINT "max="; STATS.MAX(x)
  PRINT "min="; STATS.MIN(x)
END SUB
