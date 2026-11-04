' FILE: minitests/typeinfo/typeinfo_all.bas
' Superficie TYPEINFO completa: scalari, UDT, nullable, HAS, METHODINFO/PROPERTYINFO.
' EXPECT: a=DOUBLE|b=STRING|c=Person|d=DOUBLE|e=NIL|f= 1|g= 0|h= 0|i2>0=-1|i={}|j2>0=-1|j={}
IMPORT TYPEINFO
TYPE Person
  Name AS STRING * 32
  Age AS INTEGER
END TYPE
SUB Main()
  DIM n AS INTEGER?
  DIM m AS INTEGER?
  DIM p AS Person
  n = 7
  m = NIL
  PRINT "a="; TYPEINFO(42)
  PRINT "b="; TYPEINFO("hello")
  PRINT "c="; TYPEINFO(p)
  PRINT "d="; TYPEINFO(n)
  PRINT "e="; TYPEINFO(m)
  PRINT "f="; TYPEINFO.HAS(n)
  PRINT "g="; TYPEINFO.HAS(m)
  PRINT "h="; TYPEINFO.HAS(NIL)
  PRINT "i2>0="; (LEN(METHODINFO("Person", "GetName")) > 0)   ' nome opzionale (dai Sample)
  PRINT "i="; METHODINFO("Person")
  PRINT "j2>0="; (LEN(PROPERTYINFO("Person", "Name")) > 0)   ' nome opzionale (dai Sample)
  PRINT "j="; PROPERTYINFO("Person")
END SUB
