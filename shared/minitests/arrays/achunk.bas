' EXPECT: chunks= 3
SUB Main()
  DIM a(5) = {1, 2, 3, 4, 5, 6}
  DIM c = ARRAYS.ACHUNK(a, 2)
  PRINT "chunks="; UBOUND(c) + 1
END SUB
