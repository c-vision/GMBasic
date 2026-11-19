' EXPECT: range= 9|range1= 0
IMPORT STATS
SUB Main()
  DIM x() = {4, 1, 7, 2, 9, 0, 5}
  DIM one() = {42}
  PRINT "range="; STATS.RANGE(x)
  PRINT "range1="; STATS.RANGE(one)
END SUB