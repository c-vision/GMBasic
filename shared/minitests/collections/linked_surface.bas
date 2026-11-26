' MiniTest: superficie canonica di LINKEDLIST sulle 4 gambe.
' AddLast/AddFirst/AddAfter(i,v)/AddBefore(i,v) (inserimento per INDICE 0-based),
' First/Last/Contains/IndexOf/Count/ToArray/ToList/ToJson/JSON (array),
' Sort/Reverse, RemoveFirst/RemoveLast (statement, come .NET), Remove = -1/0, Clear.
' Prima: IL non compilava (`AddAfter` non risolto per LinkedListType), PCode/VM/C
' facevano AddAfter/AddBefore no-op silenzioso su LINKEDLIST (erano gestiti solo per le
' LISTE semplici).
' EXPECT: count= 3|first= 5|last= 10|contains=-1|indexof= 2|toarr=[5,20,10]|tolist=[5,20,10]|tojson="[5,20,10]"|bare=[5,20,10]|after.addafter=[5,7,20,10]|after.addbefore=[3,5,7,20,10]|sorted=[3,5,7,10,20]|reversed=[20,10,7,5,3]|after.rem=[10,7,5]|rem=-1|final=[7,5]|cleared= 0
IMPORT JSON
IMPORT COLLECTIONS
SUB Main()
  DIM l AS LINKEDLIST<INTEGER>
  l.AddLast(20)
  l.AddLast(10)
  l.AddFirst(5)
  PRINT "count="; l.Count()
  PRINT "first="; l.First()
  PRINT "last="; l.Last()
  PRINT "contains="; l.Contains(10)
  PRINT "indexof="; l.IndexOf(10)
  PRINT "toarr="; JSON.STRINGIFY(l.ToArray())
  PRINT "tolist="; JSON.STRINGIFY(l.ToList())
  PRINT "tojson="; JSON.STRINGIFY(l.ToJson())
  PRINT "bare="; JSON.STRINGIFY(l)
  l.AddAfter(0, 7)
  PRINT "after.addafter="; JSON.STRINGIFY(l.ToArray())
  l.AddBefore(0, 3)
  PRINT "after.addbefore="; JSON.STRINGIFY(l.ToArray())
  l.Sort()
  PRINT "sorted="; JSON.STRINGIFY(l.ToArray())
  l.Reverse()
  PRINT "reversed="; JSON.STRINGIFY(l.ToArray())
  l.RemoveFirst()
  l.RemoveLast()
  PRINT "after.rem="; JSON.STRINGIFY(l.ToArray())
  PRINT "rem="; l.Remove(10)
  PRINT "final="; JSON.STRINGIFY(l.ToArray())
  l.Clear()
  PRINT "cleared="; l.Count()
END SUB
