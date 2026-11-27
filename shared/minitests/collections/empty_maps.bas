' MiniTest: mappe VUOTE — Get/MIN/MAX/First/Last = NIL su 4 gambe.
' Get(chiave assente) → NIL: PRINT vuoto, JSON null (prima IL lanciava
' NullReferenceException unboxando null a double, C99 stampava 0).
' Min/Max/First/Last di una mappa vuota → NIL (JSON null); prima C99 rendeva [].
' EXPECT: d=null// 0|t=null///|tf=null/null|bp=null// 0
IMPORT JSON
IMPORT COLLECTIONS
SUB Main()
  DIM d AS DICT<STRING, INTEGER>
  PRINT "d="; JSON.STRINGIFY(d.Get("x")); "/"; d.Get("x"); "/"; d.ContainsKey("x")
  DIM tr AS TREE<STRING, INTEGER>
  PRINT "t="; JSON.STRINGIFY(tr.Get("x")); "/"; tr.Get("x"); "/"; tr.Min(); "/"; tr.Max()
  PRINT "tf="; JSON.STRINGIFY(tr.First()); "/"; JSON.STRINGIFY(tr.Last())
  DIM bp AS BPLUSTREE<STRING, INTEGER>
  PRINT "bp="; JSON.STRINGIFY(bp.Get("x")); "/"; bp.Get("x"); "/"; bp.Contains("x")
END SUB
