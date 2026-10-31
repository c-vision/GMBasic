' MiniTest: ORDER BY / ORDER BY ... DESC nelle comprehension (`FROM n IN a ORDER BY n SELECT n`).
' Implementato nel backend C99Native (prima ORDER BY era ignorato → ordine d'inserimento).
' EXPECT: orderby-ok
SUB Main()
  DIM ok AS INTEGER = -1
  DIM a = {3, 1, 2}
  DIM asc = FROM n IN a ORDER BY n SELECT n
  IF asc.Join(",") <> "1,2,3" THEN ok = 0
  DIM desc = FROM n IN a ORDER BY n DESC SELECT n
  IF desc.Join(",") <> "3,2,1" THEN ok = 0
  IF ok THEN PRINT "orderby-ok"
END SUB
