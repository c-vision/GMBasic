' EXPECT: get1= 1|get2= 2|get3= 3|count= 3|keys1=-1
IMPORT COLLECTIONS
SUB Main()
  ' F1 — SET/GET/KEYS su DICT in TUTTE le grafe (set/Set/SET): stesso risultato.
  DIM d AS DICT OF STRING, INTEGER = NEW DICT OF STRING, INTEGER
  d.set("a", 1)
  PRINT "get1="; d.get("a")
  d.Set("b", 2)
  PRINT "get2="; d.Get("b")
  d.SET("c", 3)
  PRINT "get3="; d.GET("c")
  PRINT "count="; d.Count()
  PRINT "keys1="; d.ContainsKey("a")
END SUB
