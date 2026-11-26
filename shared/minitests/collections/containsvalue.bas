' MiniTest: ContainsKey / ContainsValue come .NET (Dictionary<TKey,TValue>).
'  - ContainsKey(x): esiste la CHIAVE x → -1/0;
'  - ContainsValue(x): esiste una voce con il VALORE x → -1/0 (scansione O(n)).
' Uguaglianza di VALORE = canone gmb_val_equals/BasicValueEquals: numerico cross-tipo
' (10 = 10.0 = TRUE), stringhe ORDINALI (case-sensitive), numero ≠ stringa,
' DateTime = seriale OADate. Vale per DICT/SORTED/TREE/AVL/REDBLACK/BPLUS/CACHE/TIMELINE.
' EXPECT: missing= 0|number=-1|double=-1|string= 0|if=yes|key=-1|keymiss= 0|dict=-1|avl=-1|avlkey=-1|cache=-1|timeline=-1|case.eq=-1|case.ne= 0
IMPORT COLLECTIONS
SUB Main()
  DIM t AS TREE<STRING, INTEGER>
  t("a") = 10
  t("b") = 20
  PRINT "missing="; t.ContainsValue(99)
  PRINT "number="; t.ContainsValue(10)
  PRINT "double="; t.ContainsValue(10.0)
  PRINT "string="; t.ContainsValue("10")
  IF t.ContainsValue(10) THEN
    PRINT "if=yes"
  ELSE
    PRINT "if=no"
  END IF
  PRINT "key="; t.ContainsKey("a")
  PRINT "keymiss="; t.ContainsKey("zzz")
  DIM d AS DICT
  d("a") = 10
  PRINT "dict="; d.ContainsValue(10)
  DIM a AS AVLTREE<INTEGER, STRING>
  a.Add(1, "a")
  PRINT "avl="; a.ContainsValue("a")
  PRINT "avlkey="; a.ContainsKey(1)
  DIM c AS CACHE<STRING, INTEGER>
  c.Set("x", 30)
  PRINT "cache="; c.ContainsValue(30)
  DIM tl AS TIMELINE<STRING>
  tl.Add(DATEVALUE("2023-01-10"), "A")
  PRINT "timeline="; tl.ContainsValue("A")
  DIM ts AS TREE<STRING, STRING>
  ts("k") = "Val"
  PRINT "case.eq="; ts.ContainsValue("Val")
  PRINT "case.ne="; ts.ContainsValue("val")
END SUB
