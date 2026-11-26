' MiniTest: superficie canonica delle MAPPE (TREE/AVL/CACHE) sulle 4 gambe.
' Canone: Keys/Values/ToArray/First/Last/Min/Max/JSON/FOR EACH nell'ordine delle CHIAVI;
' JSON.STRINGIFY = OGGETTO {"chiave":valore} (chiavi sempre stringhe in JSON, anche per
' AVLTREE<INTEGER,…> dove il valore di Keys()/FOR EACH resta NUMERICO); Remove = -1/0;
' Clear svuota. Prima: VM/C rendeva JSON null per le mappe e Last = solo valore, C99
' usava l'ordine di inserimento (b,a,c) e la chiave di FOR EACH era stringa anche per
' mappe a chiave numerica, PCode dava Values null su REDBLACK e ToArray Key:{}.
' EXPECT: tree.keys=["a","b","c"]|tree.values=[10,20,30]|tree.min=["a",10]|tree.max=["c",30]|tree.has=-1|tree.get= 10|tree.idx= 10|tree.toarr=[{"Key":"a","Value":10},{"Key":"b","Value":20},{"Key":"c","Value":30}]|tree.json={"a":10,"b":20,"c":30}|tree.entry=[[Key, a], [Value, 10]]|tree.entry=[[Key, b], [Value, 20]]|tree.entry=[[Key, c], [Value, 30]]|tree.kv=a: 10|tree.kv=b: 20|tree.kv=c: 30|tree.rem=-1|tree.final={"b":20,"c":30}|tree.cleared= 0|avl.keys=[1,2,3]|avl.values=["a","b","c"]|avl.json={"1":"a","2":"b","3":"c"}|avl.kv= 1 :a;typ=DOUBLE|avl.kv= 2 :b;typ=DOUBLE|avl.kv= 3 :c;typ=DOUBLE|cache.keys=["a","b","c"]|cache.values=[10,20,30]|cache.json={"a":10,"b":20,"c":30}|cache.entry=[[Key, a], [Value, 10]]|cache.entry=[[Key, b], [Value, 20]]|cache.entry=[[Key, c], [Value, 30]]|cache.rem=-1|cache.final={"b":20,"c":30}|cache.cleared= 0
IMPORT JSON
IMPORT COLLECTIONS
SUB Main()
  DIM t AS TREE<STRING, INTEGER>
  t("b") = 20
  t("a") = 10
  t("c") = 30
  PRINT "tree.keys="; JSON.STRINGIFY(t.Keys())
  PRINT "tree.values="; JSON.STRINGIFY(t.Values())
  PRINT "tree.min="; JSON.STRINGIFY(t.Min())
  PRINT "tree.max="; JSON.STRINGIFY(t.Max())
  PRINT "tree.has="; t.ContainsKey("a")
  PRINT "tree.get="; t.Get("a")
  PRINT "tree.idx="; t("a")
  PRINT "tree.toarr="; JSON.STRINGIFY(t.ToArray())
  PRINT "tree.json="; JSON.STRINGIFY(t)
  FOR EACH e IN t
    PRINT "tree.entry="; e
  NEXT
  FOR EACH k, v IN t
    PRINT "tree.kv="; k; ":"; v
  NEXT
  PRINT "tree.rem="; t.Remove("a")
  PRINT "tree.final="; JSON.STRINGIFY(t)
  t.Clear()
  PRINT "tree.cleared="; t.Count()

  DIM a AS AVLTREE<INTEGER, STRING>
  a.Add(2, "b")
  a.Add(1, "a")
  a.Add(3, "c")
  PRINT "avl.keys="; JSON.STRINGIFY(a.Keys())
  PRINT "avl.values="; JSON.STRINGIFY(a.Values())
  PRINT "avl.json="; JSON.STRINGIFY(a)
  FOR EACH k, v IN a
    PRINT "avl.kv="; k; ":"; v; ";typ="; TYPENAME(k)
  NEXT

  DIM c AS CACHE<STRING, INTEGER>
  c.Set("b", 20)
  c.Set("a", 10)
  c.Set("c", 30)
  PRINT "cache.keys="; JSON.STRINGIFY(c.Keys())
  PRINT "cache.values="; JSON.STRINGIFY(c.Values())
  PRINT "cache.json="; JSON.STRINGIFY(c)
  FOR EACH e IN c
    PRINT "cache.entry="; e
  NEXT
  PRINT "cache.rem="; c.Remove("a")
  PRINT "cache.final="; JSON.STRINGIFY(c)
  c.Clear()
  PRINT "cache.cleared="; c.Count()
END SUB
