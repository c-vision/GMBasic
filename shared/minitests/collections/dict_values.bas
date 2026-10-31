' MiniTest: DICT — Values()/Keys()/Get()/Count() (ordine di inserimento).
' Copre i metodi dict aggiunti all'emitter C# (prima placeholder no-op / conversione mancante).
' EXPECT: vals=[1,2]|keys=["a","b"]|g= 2|n= 2
IMPORT COLLECTIONS
IMPORT JSON

SUB Main()
  DIM d AS DICT OF STRING, INTEGER = NEW DICT OF STRING, INTEGER
  d.Set("a", 1)
  d.Set("b", 2)
  PRINT "vals="; JSON.Stringify(d.Values())
  PRINT "keys="; JSON.Stringify(d.Keys())
  PRINT "g="; d.Get("b")
  PRINT "n="; d.Count()
END SUB
