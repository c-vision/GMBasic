' EXPECT: conv_ok
' Conversioni/arrotondamento: CLNG/CINT (half-to-even), INT (floor), FIX/TRUNC (tronca),
' FLOOR, CEILING, ROUND (half-to-even). Parametri: letterali, variabili INTEGER e DOUBLE,
' negativi e semi-esatti (.5). Copre esplicitamente CLNG/CINT di un argomento INTEGER
' (prima InvalidProgramException su IL).
SUB Main()
  DIM i AS INTEGER = 7
  DIM d AS DOUBLE = 3.7
  DIM n AS DOUBLE = -2.5
  DIM ok AS INTEGER = 1
  ' CLNG: intero, double, negativo, midpoint half-to-even, argomento variabile INTEGER
  IF CLNG(i) <> 7 THEN ok = 0
  IF CLNG(d) <> 4 THEN ok = 0
  IF CLNG(n) <> -2 THEN ok = 0
  IF CLNG(0.5) <> 0 OR CLNG(1.5) <> 2 OR CLNG(2.5) <> 2 OR CLNG(3.5) <> 4 THEN ok = 0
  IF CLNG(-0.5) <> 0 OR CLNG(-3.5) <> -4 THEN ok = 0
  ' CINT: come CLNG ma sul tipo INTEGER
  IF CINT(d) <> 4 OR CINT(n) <> -2 THEN ok = 0
  IF CINT(0.5) <> 0 OR CINT(1.5) <> 2 OR CINT(2.5) <> 2 THEN ok = 0
  ' INT = floor (verso -inf) ; FIX/TRUNC = tronca verso zero
  IF INT(3.7) <> 3 OR INT(-3.7) <> -4 THEN ok = 0
  IF FIX(3.7) <> 3 OR FIX(-3.7) <> -3 THEN ok = 0
  IF TRUNC(3.7) <> 3 OR TRUNC(-3.7) <> -3 THEN ok = 0
  ' FLOOR/CEILING/ROUND (half-to-even)
  IF FLOOR(3.7) <> 3 OR FLOOR(-3.7) <> -4 THEN ok = 0
  IF CEILING(3.2) <> 4 OR CEILING(-3.2) <> -3 THEN ok = 0
  IF ROUND(2.5) <> 2 OR ROUND(3.5) <> 4 THEN ok = 0
  IF ok = 1 THEN
    PRINT "conv_ok"
  ELSE
    PRINT "conv_FAIL"
  END IF
END SUB
