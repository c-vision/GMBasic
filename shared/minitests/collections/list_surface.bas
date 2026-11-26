' MiniTest: superficie canonica di LIST sulle 4 gambe.
' Count/Contains/IndexOf/First/Last/Sum/Min/Max/Join/ToArray/ToList/ToJson/JSON
' (array, non oggetto), Sort/Reverse, indicizzazione e scrittura per indice,
' RemoveAt (statement), Remove = predicato -1/0, Clear.
' Prima: IL rendeva First/Last vuoti (valore scartato come void), Sum/Min/Max
' LEGGEVANO I BIT di un long[] come double (denormali), C99 non compilava su
' PRINT l.Remove(...) (statement-expr VOID invece del predicato).
' EXPECT: count= 3|contains=-1|indexof= 1|first= 20|last= 30|sum= 60|min= 10|max= 30|join=20-10-30|toarr=[20,10,30]|tolist=[20,10,30]|tojson="[20,10,30]"|bare=[20,10,30]|sorted=[10,20,30]|reversed=[30,20,10]|idx= 20|set=[30,99,10]|rem=-1|cleared= 0
IMPORT JSON
SUB Main()
  DIM l AS LIST<INTEGER>
  l.Add(20)
  l.Add(10)
  l.Add(30)
  PRINT "count="; l.Count()
  PRINT "contains="; l.Contains(10)
  PRINT "indexof="; l.IndexOf(10)
  PRINT "first="; l.First()
  PRINT "last="; l.Last()
  PRINT "sum="; l.Sum()
  PRINT "min="; l.Min()
  PRINT "max="; l.Max()
  PRINT "join="; l.Join("-")
  PRINT "toarr="; JSON.STRINGIFY(l.ToArray())
  PRINT "tolist="; JSON.STRINGIFY(l.ToList())
  PRINT "tojson="; JSON.STRINGIFY(l.ToJson())
  PRINT "bare="; JSON.STRINGIFY(l)
  l.Sort()
  PRINT "sorted="; JSON.STRINGIFY(l.ToArray())
  l.Reverse()
  PRINT "reversed="; JSON.STRINGIFY(l.ToArray())
  PRINT "idx="; l(1)
  l(1) = 99
  PRINT "set="; JSON.STRINGIFY(l.ToArray())
  l.RemoveAt(0)
  PRINT "rem="; l.Remove(99)
  l.Clear()
  PRINT "cleared="; l.Count()
END SUB
