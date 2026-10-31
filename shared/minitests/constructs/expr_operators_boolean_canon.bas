' MiniTest: canone booleano AND/OR/XOR con operandi confronto (2026-09-30).
' I confronti valgono -1/0 anche composti: bug IL-VarPtr-And risolto (Neg su operandi
' confronto diretti). AND/OR valore e BIT invariati: (x AND 5)=5, (6 AND 3)=3, (6 OR 0)=-1.
' EXPECT: boolcanon-ok
SUB Main()
  DIM ok AS INTEGER = -1
  IF ((2 <> 0) AND (3 <> 0)) <> -1 THEN ok = 0
  IF ((2 = 0) AND (3 <> 0)) <> 0 THEN ok = 0
  IF ((2 = 0) OR (3 <> 0)) <> -1 THEN ok = 0
  IF ((2 = 0) OR (3 = 0)) <> 0 THEN ok = 0
  IF ((2 <> 0) XOR (3 = 0)) <> -1 THEN ok = 0
  IF ((2 <> 0) XOR (3 <> 0)) <> 0 THEN ok = 0
  IF ((2 <> 0) AND 5) <> 5 THEN ok = 0
  IF (6 AND 3) <> 3 THEN ok = 0
  IF (0 AND 5) <> 0 THEN ok = 0
  IF (6 OR 0) <> -1 THEN ok = 0
  IF (0 OR 5) <> 5 THEN ok = 0
  IF (("a" = "a") AND (1 <> 0)) <> -1 THEN ok = 0
  IF (NOT (2 <> 0)) <> 0 THEN ok = 0
  DIM f AS BIT = -1
  DIM g AS BIT = 0
  IF ((2 <> 0) AND f) <> -1 THEN ok = 0
  IF (f XOR g) <> -1 THEN ok = 0
  IF ok THEN PRINT "boolcanon-ok"
END SUB
