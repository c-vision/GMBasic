' EXPECT: max= 9|min= 0
IMPORT STATS
SUB Main()
  DIM x() = {4, 1, 7, 2, 9, 0, 5}
  PRINT "max="; STATS.ARYMAX(x)
  PRINT "min="; STATS.ARYMIN(x)
END SUB