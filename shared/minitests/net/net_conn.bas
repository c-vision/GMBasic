' EXPECT: conn-ok
IMPORT NET
SUB Main()
  IF NET.CONN(0) >= 0 THEN PRINT "conn-ok"
END SUB