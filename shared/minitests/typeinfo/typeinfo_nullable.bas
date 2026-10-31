' FILE: minitests/typeinfo/typeinfo_nullable.bas
' TYPEINFO/TYPEINFO.HAS su una variabile nullable: valore presente vs NIL.
' EXPECT: v=DOUBLE|hasv= 1|nil=NIL|hasn= 0
IMPORT TYPEINFO
SUB Main()
  DIM n AS INTEGER?
  DIM m AS INTEGER?
  n = 7
  m = NIL
  PRINT "v="; TYPEINFO(n)
  PRINT "hasv="; TYPEINFO.HAS(n)
  PRINT "nil="; TYPEINFO(m)
  PRINT "hasn="; TYPEINFO.HAS(m)
END SUB
