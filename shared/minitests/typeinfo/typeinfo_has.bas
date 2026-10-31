' FILE: minitests/typeinfo/typeinfo_has.bas
' TYPEINFO.HAS(x) — 1 se il valore non è NIL, 0 se NIL.
' EXPECT: h1= 1|h2= 1|h3= 0
IMPORT TYPEINFO
SUB Main()
  PRINT "h1="; TYPEINFO.HAS(42)
  PRINT "h2="; TYPEINFO.HAS("x")
  PRINT "h3="; TYPEINFO.HAS(NIL)
END SUB
