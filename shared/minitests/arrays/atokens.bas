' EXPECT: n= 3|a|b|c
SUB Main()
  DIM t = ARRAYS.ATOKENS("a,,b;c", ",;")
  PRINT "n="; UBOUND(t) + 1
  PRINT t(0); "|"; t(1); "|"; t(2)
END SUB
