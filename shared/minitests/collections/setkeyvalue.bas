' EXPECT: 3|count= 1|has= 0
IMPORT COLLECTIONS
SUB Main()
  ' DICTOFSETKEYVALUE: SetKeyValue(k, v) — alias semantico di Set/Add (sovrascrive).
  DIM d AS DICT OF STRING, INTEGER = NEW DICT OF STRING, INTEGER
  d.SetKeyValue("a", 2)
  d.SetKeyValue("a", 3)
  PRINT d.Get("a")
  PRINT "count="; d.Count()
  PRINT "has="; d.ContainsKey("b")
END SUB
