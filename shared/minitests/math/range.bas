' EXPECT: n= 5 |r0= 1 |r4= 5|rstep= 3
IMPORT MATH
SUB Main()
  DIM r = MATH.RANGE(1, 5)
  PRINT "n="; LEN(r); "|r0="; r(0); "|r4="; r(4)
  DIM s = MATH.RANGE(1, 5, 2)
  PRINT "rstep="; LEN(s)
END SUB
