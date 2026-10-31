' EXPECT: n= 3|beta|alpha|gamma
SUB Main()
  DIM n$()
  AADD n$, "beta"
  AADD n$, "alpha"
  AADD n$, "gamma"
  PRINT "n="; UBOUND(n$) + 1
  PRINT n$(0); "|"; n$(1); "|"; n$(2)
END SUB
