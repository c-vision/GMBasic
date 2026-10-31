' EXPECT: n= 3|t=a,b,c
SUB Main()
  DIM t = ATOKENS("a,b,c", ",")
  PRINT "n="; UBOUND(t) + 1
  PRINT "t="; t(0); ","; t(1); ","; t(2)
END SUB
