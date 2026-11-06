' EXPECT: max= 9|min= 0
IMPORT STATSX
SUB Main()
  DIM x() = {4, 1, 7, 2, 9, 0, 5}
  PRINT "max="; STATSX.MAX(x)
  PRINT "min="; STATSX.MIN(x)
END SUB