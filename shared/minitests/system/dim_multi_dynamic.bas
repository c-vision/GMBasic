' EXPECT: a= 1  b= 2
' DIM a$(), b$() AS STRING: ENTRAMBI dinamici (il parser C marcava solo il primo).
SUB Main()
  DIM a$(), b$() AS STRING
  a$ = SPLIT("x,y", ",")
  b$ = SPLIT("p,q,r", ",")
  PRINT "a="; UBOUND(a$); " b="; UBOUND(b$)
END SUB
