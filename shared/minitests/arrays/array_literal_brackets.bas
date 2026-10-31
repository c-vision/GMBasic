' EXPECT: brackets_ok
' Inizializzatore di array con parentesi quadre `[ ... ]` (ListLiteralExpr) come `{ ... }`:
' implicito (size = n. elementi) e dimensionato (primi elementi dai valori), DOUBLE/INTEGER/STRING.
SUB Main()
  DIM a() AS DOUBLE = [1.5, 2.5, 3.5]
  DIM b(4) AS DOUBLE = [10.0, 20.0, 30.0]
  DIM c() AS INTEGER = [1, 2, 3, 4]
  DIM s() AS STRING = ["aa", "bb", "cc"]
  DIM mx(1, 1) AS INTEGER = [[1, 2], [3, 4]]
  DIM ok AS INTEGER = 1
  IF a(0) <> 1.5 OR a(2) <> 3.5 THEN ok = 0
  IF b(0) <> 10.0 OR b(2) <> 30.0 OR b(4) <> 0.0 THEN ok = 0
  IF c(0) <> 1 OR c(3) <> 4 THEN ok = 0
  IF s(0) <> "aa" OR s(2) <> "cc" THEN ok = 0
  IF mx(0,0) <> 1 OR mx(0,1) <> 2 OR mx(1,0) <> 3 OR mx(1,1) <> 4 THEN ok = 0
  IF ok = 1 THEN
    PRINT "brackets_ok"
  ELSE
    PRINT "brackets_FAIL"
  END IF
END SUB
