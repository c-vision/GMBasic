' MiniTest: Get su mappe a VALORI STRINGA — NIL quando la chiave manca (4 gambe).
' DICT/TREE/TIMELINE<...,STRING>: JSON.STRINGIFY(Get(assente)) = null, PRINT vuoto e
' la concatenazione lo tratta come "" (prima C99 rendeva "" nel JSON e "<nil>" in
' concatenazione). Con la chiave presente il valore è la stringa.
' EXPECT: d0=null//[]|d1="v"/v/[v]|t0=null/|tl0=null/
IMPORT JSON
IMPORT COLLECTIONS
SUB Main()
  DIM d AS DICT<STRING, STRING>
  PRINT "d0="; JSON.STRINGIFY(d.Get("x")); "/"; d.Get("x"); "/"; "["+d.Get("x")+"]"
  d("k") = "v"
  PRINT "d1="; JSON.STRINGIFY(d.Get("k")); "/"; d.Get("k"); "/"; "["+d.Get("k")+"]"
  DIM t AS TREE<STRING, STRING>
  PRINT "t0="; JSON.STRINGIFY(t.Get("x")); "/"; t.Get("x")
  DIM tl AS TIMELINE<STRING>
  PRINT "tl0="; JSON.STRINGIFY(tl.Get(NOW())); "/"; tl.Get(NOW())
END SUB
