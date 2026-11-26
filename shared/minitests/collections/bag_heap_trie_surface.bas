' MiniTest: superficie canonica di BAG / HEAP / TRIE sulle 4 gambe.
' BAG (multiset): Count()/Count(x)/UniqueCount()/Contains() numerici; ToArray/ToList =
'   coppie {Key:elemento, Value:molteplicità}; UniqueItems = elementi distinti; ToJson e
'   JSON.STRINGIFY = oggetto {elemento:molteplicità}; Remove toglie UNA istanza (-1/0).
' HEAP: Peek/Extract rendono il valore della radice (double); ToArray/ToList/ToJson = ordine
'   dell'ARRAY interno dell'heap (heapify, canone CommonLib BinaryHeap di IL/PCode).
' TRIE: Contains/StartsWith predicati -1/0; FindAll/ToArray/ToList = parole; ToJson = array;
'   Remove rende -1 se la parola esisteva e la toglie anche da ToArray/JSON.
' Prima: PCode rendeva null per Trie ToArray/ToList, IL usciva con EntryPointNotFoundException
' sul BAG ToList, VM/C dava l'ordine di inserimento (non heapify) e valori grezzi per il BAG,
' C99 non compilava (gmb_print_val su double, gmb_make_val_list su handle).
' EXPECT: b.count= 5|b.count.a= 3|b.uniqcount= 3|b.contains=-1|b.toarr=[{"Key":"a","Value":3},{"Key":"b","Value":1},{"Key":"c","Value":1}]|b.tolist=[{"Key":"a","Value":3},{"Key":"b","Value":1},{"Key":"c","Value":1}]|b.unique=["a","b","c"]|b.tojson={"a":3,"b":1,"c":1}|b.bare={"a":3,"b":1,"c":1}|b.rem=-1|b.after.rem={"a":2,"b":1,"c":1}|b.cleared= 0|h.count= 3|h.peek= 10|h.toarr=[10,30,20]|h.tojson=[10,30,20]|h.bare=[10,30,20]|h.extract= 10|h.after=[20,30]|t.count= 3|t.contains=-1|t.startswith=-1|t.findall=["car","cat"]|t.toarr=["car","cat","dog"]|t.tojson=["car","cat","dog"]|t.rem=-1|t.final=["car","cat"]|t.cleared= 0
IMPORT JSON
IMPORT COLLECTIONS
SUB Main()
  DIM b AS BAG<STRING>
  b.Add("a")
  b.Add("b")
  b.Add("a")
  b.Add("c")
  b.Add("a")
  PRINT "b.count="; b.Count()
  PRINT "b.count.a="; b.Count("a")
  PRINT "b.uniqcount="; b.UniqueCount()
  PRINT "b.contains="; b.Contains("b")
  PRINT "b.toarr="; JSON.STRINGIFY(b.ToArray())
  PRINT "b.tolist="; JSON.STRINGIFY(b.ToList())
  PRINT "b.unique="; JSON.STRINGIFY(b.UniqueItems())
  PRINT "b.tojson="; b.ToJson()
  PRINT "b.bare="; JSON.STRINGIFY(b)
  PRINT "b.rem="; b.Remove("a")
  PRINT "b.after.rem="; b.ToJson()
  b.Clear()
  PRINT "b.cleared="; b.Count()

  DIM h AS HEAP<INTEGER>
  h.Add(30)
  h.Add(10)
  h.Add(20)
  PRINT "h.count="; h.Count()
  PRINT "h.peek="; h.Peek()
  PRINT "h.toarr="; JSON.STRINGIFY(h.ToArray())
  PRINT "h.tojson="; h.ToJson()
  PRINT "h.bare="; JSON.STRINGIFY(h)
  PRINT "h.extract="; h.Extract()
  PRINT "h.after="; JSON.STRINGIFY(h.ToArray())

  DIM t AS TRIE
  t.Add("car")
  t.Add("cat")
  t.Add("dog")
  PRINT "t.count="; t.Count()
  PRINT "t.contains="; t.Contains("car")
  PRINT "t.startswith="; t.StartsWith("ca")
  PRINT "t.findall="; JSON.STRINGIFY(t.FindAll("ca"))
  PRINT "t.toarr="; JSON.STRINGIFY(t.ToArray())
  PRINT "t.tojson="; t.ToJson()
  PRINT "t.rem="; t.Remove("dog")
  PRINT "t.final="; JSON.STRINGIFY(t.ToArray())
  t.Clear()
  PRINT "t.cleared="; t.Count()
END SUB
