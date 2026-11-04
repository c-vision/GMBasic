' MiniTest COLLECTIONS — HLL (HyperLogLog): stima della cardinalità (distinct count).
' Copre TUTTI i metodi introdotti: Add, Count, Merge, Error, Precision, Serialize, Load,
' Clear + TYPENAME. Verifica anche il payload canonico (1 e 1.0 sono lo STESSO valore;
' la stringa "1" è un valore DIVERSO dal numero 1) e l'accuratezza su 10.000 valori
' distinti (linear counting / HLL: valore deterministico perché l'hash è fisso).
'
' EXPECT: p= 14|e= 0.008125|c0= 0|c2= 2|cp= 2|cNum= 2|cStr= 2|rt= 2|clr= 0|big= 9973|mrg= 9973|t=HLL|e1=OK
IMPORT COLLECTIONS

SUB Main()
  DIM h AS HLL
  PRINT "p="; h.Precision()
  PRINT "e="; h.Error()
  PRINT "c0="; h.Count()

  ' Add: i duplicati non contano
  h.Add("a") : h.Add("b") : h.Add("a")
  PRINT "c2="; h.Count()
  ' forma di PROPRIETÀ (senza parentesi): stesso valore di Count()
  PRINT "cp="; h.Count

  ' Payload canonico: numero 1 e 1.0 = stesso valore; 2 distinto
  DIM hn AS HLL
  hn.Add(1) : hn.Add(1.0) : hn.Add(2)
  PRINT "cNum="; hn.Count()

  ' STRINGA "1" diversa dal numero 1
  DIM hs AS HLL
  hs.Add(1) : hs.Add("1")
  PRINT "cStr="; hs.Count()

  ' Serialize/Load round-trip
  DIM blob AS STRING
  blob = h.Serialize()
  DIM h2 AS HLL
  h2.Load(blob)
  PRINT "rt="; h2.Count()

  ' Clear
  h2.Clear()
  PRINT "clr="; h2.Count()

  ' Accuratezza: 10.000 valori distinti
  DIM big AS HLL
  DIM i AS INTEGER
  FOR i = 1 TO 10000
    big.Add(i)
  NEXT
  PRINT "big="; big.Count()

  ' Merge: due sketch da 5.000 valori ciascuno → stesso insieme di 10.000
  DIM s1 AS HLL : DIM s2 AS HLL
  FOR i = 1 TO 5000
    s1.Add(i)
  NEXT
  FOR i = 5001 TO 10000
    s2.Add(i)
  NEXT
  s1.Merge(s2)
  PRINT "mrg="; s1.Count()

  ' TYPENAME
  PRINT "t="; TYPENAME(h)

  ' Error() deve corrispondere a 1.04/sqrt(2^p) (0.008125) — confronto a tolleranza
  IF ABS(h.Error() - 0.008125) <= 0.000001 THEN PRINT "e1=OK" ELSE PRINT "e1=BAD"
END SUB
