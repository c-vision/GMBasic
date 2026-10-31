' EXPECT: nzip= 3
SUB Main()
  DIM a1(2) = {10, 20, 30}
  DIM a2(2) = {100, 200, 300}
  DIM z = ARRAYS.AZIP(a1, a2)
  PRINT "nzip="; UBOUND(z) + 1
END SUB
