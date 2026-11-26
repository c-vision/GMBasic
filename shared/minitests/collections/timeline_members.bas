' MiniTest: superficie COMPLETA dei membri di TIMELINE sulle 4 gambe.
' Canone: chiave = DATA; Keys/First/Last/GetRange/ToArray/FOR EACH usano il SERIALE
' OADate (numeri), ContainsKey/Contains = -1/0, Get/indexer = valore o NIL,
' ToJson/JSON.STRINGIFY = OGGETTO con chiavi ISO-8601 (come le chiavi-data dei DICT),
' t(data)=v = Add con sovrascrittura, Remove = -1/0, Clear svuota.
' Prima: IL non compilava (ContainsKey/Get/indexer), PCode rendeva null su
' ContainsKey/Values e Key={} su ToArray, VM/C dava Last = solo valore e JSON null,
' C99 non gestiva Contains/Clear, la chiave dell'indexer e FOR EACH erano stringhe.
' EXPECT: count= 2|keys=[44936,44946]|values=["A","B"]|has1=-1|has2=-1|has3= 0|get=B|idx=B|first={"Key":44936,"Value":"A"}|last={"Key":44946,"Value":"B"}|range=[{"Key":44936,"Value":"A"},{"Key":44946,"Value":"B"}]|toarr=[{"Key":44936,"Value":"A"},{"Key":44946,"Value":"B"}]|tojson="{\u00222023-01-10T00:00:00\u0022:\u0022A\u0022,\u00222023-01-20T00:00:00\u0022:\u0022B\u0022}"|bare={"2023-01-10T00:00:00":"A","2023-01-20T00:00:00":"B"}|entry=[[Key, 44936], [Value, A]]|entry=[[Key, 44946], [Value, B]]|kv= 44936 :A;typ=DOUBLE|kv= 44946 :B;typ=DOUBLE|key= 44936|key= 44946|after={"2023-01-10T00:00:00":"C","2023-01-20T00:00:00":"B"}|rem=-1|rem2= 0|final={"2023-01-20T00:00:00":"B"}|cleared= 0
IMPORT JSON
IMPORT COLLECTIONS
SUB Main()
  DIM t AS TIMELINE<STRING>
  DIM d1 AS DATETIME
  DIM d2 AS DATETIME
  d1 = DATEVALUE("2023-01-10")
  d2 = DATEVALUE("2023-01-20")
  t.Add(d1, "A")
  t.Add(d2, "B")
  PRINT "count="; t.Count()
  PRINT "keys="; JSON.STRINGIFY(t.Keys())
  PRINT "values="; JSON.STRINGIFY(t.Values())
  PRINT "has1="; t.ContainsKey(d1)
  PRINT "has2="; t.Contains(d2)
  PRINT "has3="; t.ContainsKey(DATEVALUE("2024-01-01"))
  PRINT "get="; t.Get(d2)
  PRINT "idx="; t(d2)
  PRINT "first="; JSON.STRINGIFY(t.First())
  PRINT "last="; JSON.STRINGIFY(t.Last())
  PRINT "range="; JSON.STRINGIFY(t.GetRange(DATEVALUE("2023-01-01"), DATEVALUE("2023-01-31")))
  PRINT "toarr="; JSON.STRINGIFY(t.ToArray())
  PRINT "tojson="; JSON.STRINGIFY(t.ToJson())
  PRINT "bare="; JSON.STRINGIFY(t)
  FOR EACH e IN t
    PRINT "entry="; e
  NEXT
  FOR EACH k, v IN t
    PRINT "kv="; k; ":"; v; ";typ="; TYPENAME(k)
  NEXT
  FOR EACH KEY k IN t
    PRINT "key="; k
  NEXT
  t(d1) = "C"
  PRINT "after="; JSON.STRINGIFY(t)
  PRINT "rem="; t.Remove(d1)
  PRINT "rem2="; t.Remove(d1)
  PRINT "final="; JSON.STRINGIFY(t)
  t.Clear()
  PRINT "cleared="; t.Count()
END SUB
