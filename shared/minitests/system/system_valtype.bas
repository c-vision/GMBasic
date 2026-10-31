' SYSTEM.VALTYPE(expr) — lettera di tipo Clipper: N (numerico), C (stringa), L (logico), U (nil).
' EXPECT: num= N|str= C
IMPORT SYSTEM
SUB Main()
  IF SYSTEM.VALTYPE(1) = "N" THEN PRINT "num= N" ELSE PRINT "num= ?"
  IF SYSTEM.VALTYPE("x") = "C" THEN PRINT "str= C" ELSE PRINT "str= ?"
END SUB
