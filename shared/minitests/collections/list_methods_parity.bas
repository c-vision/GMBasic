' MiniTest: metodi di LISTA/ARRAY nativi su array dinamico (`DIM t = []`).
' Copre ogni comando/funzione implementato nel backend C99Native (parità con IL/PCode/
' VM/C): Add(x), Count(), Join(sep), Max(), Min(), Sum(), Sort(), Clear().
'  - Add: 1 argomento (valore). Count/Max/Min/Sum: nessun argomento. Join: 1 (separatore).
'  - Sort/Clear: nessun argomento, mutano la lista.
' EXPECT: listmethods-ok
SUB Main()
  DIM ok AS INTEGER = -1
  DIM t = []
  t.Add(30)
  t.Add(10)
  t.Add(20)
  IF t.Count() <> 3 THEN ok = 0
  IF t.Join(",") <> "30,10,20" THEN ok = 0
  IF t.Max() <> 30 THEN ok = 0
  IF t.Min() <> 10 THEN ok = 0
  IF t.Sum() <> 60 THEN ok = 0
  t.Sort()
  IF t.Join(",") <> "10,20,30" THEN ok = 0
  t.Clear()
  IF t.Count() <> 0 THEN ok = 0
  IF ok THEN PRINT "listmethods-ok"
END SUB
