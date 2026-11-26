' MiniTest: superficie canonica di HASHSET sulle 4 gambe.
' Elementi UNICI in ORDINE DI INSERIMENTO (canone del linguaggio: C99/VM/C usano liste
' ordinate per inserimento, IL/PCode set ordinati); Union/Intersect/Except/SymmetricExcept
' rendono un NUOVO insieme con elementi di A nell'ordine di A, poi quelli di B non in A.
' Clear svuota; ToArray/ToJson/JSON = array dei valori.
' Prima: PCode usava HashSet (ordine hash) per i risultati → [2,9,3] vs [9,3,2];
' Clear su HASHSET era un no-op silenzioso su PCode (Count restava); VM/C rendeva null
' per JSON/bare e per gli insiemi risultanti.
' EXPECT: h.count= 2|h.contains=-1|h.contains.no= 0|h.remove=-1|h.remove.no= 0|h.toarr=[20]|h.tolist=[20]|h.tojson="[20]"|h.bare=[20]|h.union=[20,30]|h.inter=[20]|h.except=[]|h.cleared= 0|a1=[5,9,3]|a2=[9,3]|u2=[9,3,2]|i2=[9]|e2=[3]|s2=[3,2]
IMPORT JSON
SUB Main()
  DIM h AS HASHSET<INTEGER>
  h.Add(10)
  h.Add(20)
  h.Add(10)
  PRINT "h.count="; h.Count()
  PRINT "h.contains="; h.Contains(20)
  PRINT "h.contains.no="; h.Contains(99)
  PRINT "h.remove="; h.Remove(10)
  PRINT "h.remove.no="; h.Remove(99)
  PRINT "h.toarr="; JSON.STRINGIFY(h.ToArray())
  PRINT "h.tolist="; JSON.STRINGIFY(h.ToList())
  PRINT "h.tojson="; JSON.STRINGIFY(h.ToJson())
  PRINT "h.bare="; JSON.STRINGIFY(h)
  DIM o AS HASHSET<INTEGER>
  o.Add(20)
  o.Add(30)
  PRINT "h.union="; JSON.STRINGIFY(h.Union(o))
  PRINT "h.inter="; JSON.STRINGIFY(h.Intersect(o))
  PRINT "h.except="; JSON.STRINGIFY(h.Except(o))
  h.Clear()
  PRINT "h.cleared="; h.Count()

  DIM s AS HASHSET<INTEGER>
  s.Add(5)
  s.Add(1)
  s.Add(9)
  s.Add(3)
  s.Remove(1)
  PRINT "a1="; JSON.STRINGIFY(s.ToArray())
  s.Remove(5)
  PRINT "a2="; JSON.STRINGIFY(s.ToArray())
  DIM t AS HASHSET<INTEGER>
  t.Add(2)
  t.Add(9)
  PRINT "u2="; JSON.STRINGIFY(s.Union(t))
  PRINT "i2="; JSON.STRINGIFY(s.Intersect(t))
  PRINT "e2="; JSON.STRINGIFY(s.Except(t))
  PRINT "s2="; JSON.STRINGIFY(s.SymmetricExcept(t))
END SUB
