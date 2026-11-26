' MiniTest: DOUBLYLINKEDLIST e CIRCULAR sulle 4 gambe.
' DOUBLYLINKEDLIST: stessa superficie di LINKEDLIST (incluso AddAfter per indice).
' CIRCULAR: Add/Current/ToArray/ToJson/Rotate(1)/Contains/Remove/Clear — Rotate sposta la
' testa in avanti di N (circolare: A,B,C → B,C,A).
' Prima: `DIM d AS DOUBLYLINKEDLIST<T>` SENZA inizializzatore restava NIL su
' PCode/VM/C (mancava nella lista "needsInit" del pre-pass locale) e su IL il DIM
' materializzava un SortedDictionary (local di tipo sbagliato: Count garbage e
' AccessViolation al primo uso).
' EXPECT: d.count= 3|d.first= 5|d.last= 10|d.contains=-1|d.indexof= 2|d.toarr=[5,20,10]|d.tojson="[5,20,10]"|d.bare=[5,20,10]|d.addafter=[5,7,20,10]|d.sorted=[5,7,10,20]|d.reversed=[20,10,7,5]|d.after.rem=[10,7]|d.rem=-1|d.final=[7]|d.cleared= 0|c.count= 3|c.current=A|c.toarr=["A","B","C"]|c.tojson="[\u0022A\u0022,\u0022B\u0022,\u0022C\u0022]"|c.rotated=["B","C","A"]|c.contains=-1|c.rem=-1|c.final=["C","A"]|c.cleared= 0
IMPORT JSON
IMPORT COLLECTIONS
SUB Main()
  DIM d AS DOUBLYLINKEDLIST<INTEGER>
  d.AddLast(20)
  d.AddLast(10)
  d.AddFirst(5)
  PRINT "d.count="; d.Count()
  PRINT "d.first="; d.First()
  PRINT "d.last="; d.Last()
  PRINT "d.contains="; d.Contains(10)
  PRINT "d.indexof="; d.IndexOf(10)
  PRINT "d.toarr="; JSON.STRINGIFY(d.ToArray())
  PRINT "d.tojson="; JSON.STRINGIFY(d.ToJson())
  PRINT "d.bare="; JSON.STRINGIFY(d)
  d.AddAfter(0, 7)
  PRINT "d.addafter="; JSON.STRINGIFY(d.ToArray())
  d.Sort()
  PRINT "d.sorted="; JSON.STRINGIFY(d.ToArray())
  d.Reverse()
  PRINT "d.reversed="; JSON.STRINGIFY(d.ToArray())
  d.RemoveFirst()
  d.RemoveLast()
  PRINT "d.after.rem="; JSON.STRINGIFY(d.ToArray())
  PRINT "d.rem="; d.Remove(10)
  PRINT "d.final="; JSON.STRINGIFY(d.ToArray())
  d.Clear()
  PRINT "d.cleared="; d.Count()

  DIM c AS CIRCULAR<STRING>
  c.Add("A")
  c.Add("B")
  c.Add("C")
  PRINT "c.count="; c.Count()
  PRINT "c.current="; c.Current()
  PRINT "c.toarr="; JSON.STRINGIFY(c.ToArray())
  PRINT "c.tojson="; JSON.STRINGIFY(c.ToJson())
  c.Rotate(1)
  PRINT "c.rotated="; JSON.STRINGIFY(c.ToArray())
  PRINT "c.contains="; c.Contains("B")
  PRINT "c.rem="; c.Remove("B")
  PRINT "c.final="; JSON.STRINGIFY(c.ToArray())
  c.Clear()
  PRINT "c.cleared="; c.Count()
END SUB
