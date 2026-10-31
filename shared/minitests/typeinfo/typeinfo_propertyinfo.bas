' FILE: minitests/typeinfo/typeinfo_propertyinfo.bas
' PROPERTYINFO(tipo) — JSON delle proprietà: `{}` (parità con METHODINFO).
' EXPECT: p={}
IMPORT TYPEINFO
SUB Main()
  PRINT "p="; TYPEINFO.PROPERTYINFO("Person")
END SUB
