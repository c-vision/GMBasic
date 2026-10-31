' MiniTest: EXIT/CONTINUE con ETICHETTA di loop (canone 2026-09-30).
'  - `EXIT LBL` esce dal loop etichettato (prima su emitter C# usciva dal loop interno,
'    e su C99Native dalla SUB).
'  - `CONTINUE LBL` rientra all'iterazione successiva del loop etichettato.
' EXPECT: labels-ok
SUB Main()
  DIM ok AS INTEGER = -1
  DIM outerSeen AS INTEGER = 0
  OO: FOR I = 1 TO 3
    II: FOR J = 1 TO 3
      outerSeen = outerSeen + 1
      IF J = 2 THEN EXIT OO
    NEXT J
  NEXT I
  IF outerSeen <> 2 THEN ok = 0

  DIM cont AS INTEGER = 0
  C1: FOR I = 1 TO 3
    C2: FOR J = 1 TO 3
      cont = cont + 1
      IF J = 2 THEN CONTINUE C1
    NEXT J
  NEXT I
  IF cont <> 6 THEN ok = 0
  IF ok THEN PRINT "labels-ok"
END SUB
