' FILE: minitests/typeinfo/typeinfo_udt.bas
' TYPEINFO(udt) — nome del TYPE utente (non il nome CLR interno "UDT_<id>").
' EXPECT: udt=Person
IMPORT TYPEINFO
TYPE Person
  Name AS STRING * 32
  Age AS INTEGER
END TYPE
SUB Main()
  DIM p AS Person
  PRINT "udt="; TYPEINFO(p)
END SUB
