' EXPECT: range= 9|range1= 0
IMPORT STATSX
SUB Main()
  DIM x() = {4, 1, 7, 2, 9, 0, 5}
  DIM one() = {42}
  PRINT "range="; STATSX.RANGE(x)
  PRINT "range1="; STATSX.RANGE(one)
END SUB