' EXPECT: rev=b,a,c|padN= 4|pad3=z
SUB Main()
  DIM w(2) AS STRING
  w(0) = "c"
  w(1) = "a"
  w(2) = "b"
  DIM r$()
  r$ = ARRAYS.AREVERSE(w)
  PRINT "rev="; r$(0); ","; r$(1); ","; r$(2)
  DIM p$()
  p$ = ARRAYS.APAD(w, 4, "z")
  PRINT "padN="; UBOUND(p$) + 1
  PRINT "pad3="; p$(3)
END SUB
