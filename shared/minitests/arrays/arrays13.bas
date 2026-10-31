' EXPECT: a|b|c|n= 3|a|b|d
SUB Main()
  DIM l$()
  AADD l$, "a"
  AADD l$, "c"
  AINS l$, 1
  l$(1) = "b"
  PRINT l$(0); "|"; l$(1); "|"; l$(2)
  AADD l$, "d"
  ADEL l$, 2
  PRINT "n="; UBOUND(l$) + 1
  PRINT l$(0); "|"; l$(1); "|"; l$(2)
END SUB
