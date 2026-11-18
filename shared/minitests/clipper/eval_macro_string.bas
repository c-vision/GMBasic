' GMBASIC (piu' flessibile) — EVAL("expr") macro di stringa coesiste con EVAL(block,args)
' EXPECT: s1= 21|s2= 5|s3= 42|s4= 42
GLOBAL Y AS INTEGER
SUB Main()
  Y = 7
  PRINT "s1="; EVAL("Y * 3")     ' 21
  PRINT "s2="; EVAL("2+3")       ' 5
  PRINT "s3="; EVAL("Y = 42")    ' 42 (comando); Y diventa 42
  PRINT "s4="; Y                 ' 42
END SUB
