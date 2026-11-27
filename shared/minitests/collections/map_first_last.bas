' MiniTest: First/Last sulle MAPPE (TREE/AVL/CACHE) e sulle SEQUENZE (LIST) — 4 gambe.
' Canone: mappa.First()/Last() = coppia [chiave, valore] in ORDINE DI CHIAVE (≡ Min/Max);
' Timeline.First()/Last() = oggetto {Key,Value}; sequenza.First()/Last() = elemento di
' testa/coda. Cache: ordine canonico di chiave anche se lo storage è hash.
' Prima: IL non compilava (`t.First()` → "campo/proprietà 'First' non risolvibile" e
' l'IndexedAccess a 0 indici scartava il valore); PCode rendeva null (LOAD_FIELD + 0 indici
' e First/Last fuori schema); VM/C rendeva null (OP_LOAD_FIELD su collezione non gestiva
' First/Last, e l'element-load a 0 indici scartava la coppia); Cache.Last sbagliato su IL/PCode
' (Entries non ordinate).
' EXPECT: tree.first=["a",10]|tree.last=["c",30]|avl.first=[1,"a"]|avl.last=[3,"c"]|cache.first=["x",1]|cache.last=["z",3]|list.first= 10|list.last= 30
IMPORT JSON
IMPORT COLLECTIONS
SUB Main()
  DIM t AS TREE<STRING, INTEGER>
  t("b") = 20 : t("a") = 10 : t("c") = 30
  PRINT "tree.first="; JSON.STRINGIFY(t.First())
  PRINT "tree.last="; JSON.STRINGIFY(t.Last())
  DIM a AS AVLTREE<INTEGER, STRING>
  a(2) = "b" : a(1) = "a" : a(3) = "c"
  PRINT "avl.first="; JSON.STRINGIFY(a.First())
  PRINT "avl.last="; JSON.STRINGIFY(a.Last())
  DIM c AS CACHE<STRING, INTEGER>
  c.Set("x", 1) : c.Set("y", 2) : c.Set("z", 3)
  PRINT "cache.first="; JSON.STRINGIFY(c.First())
  PRINT "cache.last="; JSON.STRINGIFY(c.Last())
  DIM l AS LIST<INTEGER>
  l.Add(10) : l.Add(20) : l.Add(30)
  PRINT "list.first="; l.First()
  PRINT "list.last="; l.Last()
END SUB
