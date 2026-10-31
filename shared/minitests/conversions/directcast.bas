' MiniTest DIRECTCAST — estrazione del valore al tipo target (numerico e stringa).
' Solo confronti (niente PRINT numerico: formattazione del segno diverge fra gambe).
' EXPECT: i=OK|d=OK|s=OK|r=OK
SUB Main()
  IF DIRECTCAST("42", LONG) = 42 THEN PRINT "i=OK"
  IF ABS(DIRECTCAST("3.5", DOUBLE) - 3.5) < 0.0001 THEN PRINT "d=OK"
  IF DIRECTCAST("abc", STRING) = "abc" THEN PRINT "s=OK"
  IF DIRECTCAST(7, INTEGER) = 7 THEN PRINT "r=OK"
END SUB
