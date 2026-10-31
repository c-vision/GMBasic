' EXPECT: rev=b,a,c|n= 5|p4=x|join=c-a-b
SUB Main()
  DIM w(2) AS STRING
  w(0) = "c": w(1) = "a": w(2) = "b"
  DIM r$ = ARRAYS.AREVERSE(w)
  PRINT "rev="; r$(0); ","; r$(1); ","; r$(2)
  DIM p$ = ARRAYS.APAD(w, 5, "x")
  PRINT "n="; UBOUND(p$) + 1
  PRINT "p4="; p$(4)
  PRINT "join="; ARRAYS.AJOIN(w, "-")
END SUB
