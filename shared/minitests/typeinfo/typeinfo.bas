' FILE: minitests/typeinfo/typeinfo.bas
' TYPEINFO(x) su scalari — nome del tipo del valore (parità 4 backend).
' EXPECT: num=DOUBLE|dbl=DOUBLE|str=STRING
IMPORT TYPEINFO
SUB Main()
  PRINT "num="; TYPEINFO(42)
  PRINT "dbl="; TYPEINFO(3.5)
  PRINT "str="; TYPEINFO("hello")
END SUB
