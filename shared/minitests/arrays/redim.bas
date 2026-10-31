' EXPECT: ub= 4|0  0  0|1  3  0
SUB Main()
  DIM a(2) = {1, 2, 3}
  REDIM a(4)
  PRINT "ub="; UBOUND(a)
  PRINT a(0); a(3); a(4)
  DIM b(2) = {1, 2, 3}
  REDIM PRESERVE b(4)
  PRINT b(0); b(2); b(4)
END SUB
