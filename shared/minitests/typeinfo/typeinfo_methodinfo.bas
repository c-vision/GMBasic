' FILE: minitests/typeinfo/typeinfo_methodinfo.bas
' METHODINFO(tipo) — JSON dei metodi: `{}` per un tipo non nel TypeSchemaRegistry C#.
' EXPECT: m={}
IMPORT TYPEINFO
SUB Main()
  PRINT "m="; TYPEINFO.METHODINFO("Person")
END SUB
