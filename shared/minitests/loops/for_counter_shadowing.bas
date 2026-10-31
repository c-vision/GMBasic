' MiniTest: canone shadowing contatore FOR (2026-09-30, bug FOR-shadowing risolto).
' Il contatore e locale al blocco: una DIM omonima esterna conserva il suo valore
' dopo il ciclo, in SUB come a top-level; FOR sequenziali/annidati non interferiscono.
' EXPECT: forshadow-ok
SUB Main()
  DIM ok AS INTEGER = -1
  DIM M = 42
  FOR M = 1 TO 3
  NEXT M
  IF M <> 42 THEN ok = 0
  DIM innerSeen AS INTEGER = 0
  FOR M = 1 TO 2
    FOR N = 1 TO 2
      innerSeen = innerSeen + N
    NEXT N
  NEXT M
  IF M <> 42 THEN ok = 0
  IF innerSeen <> 6 THEN ok = 0
  DIM S = 7
  FOR S = 1 TO 2
  NEXT S
  FOR S = 1 TO 2
  NEXT S
  IF S <> 7 THEN ok = 0
  IF ok THEN PRINT "forshadow-ok"
END SUB
