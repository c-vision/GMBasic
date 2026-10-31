' EXPECT: shared_ok
' DIM multi-variabile con `AS` CONDIVISO: `DIM a, b AS STRING` tipa ENTRAMBE le variabili
' (canone classico, parity C99Native). Prima il/pcode/vmc/c99 davano STRING solo all'ultima
' (`a` restava DOUBLE → `a = "x"` = errore di compilazione).
SUB Main()
  DIM a, b AS STRING
  a = "x"
  b = "y"
  DIM n, m AS INTEGER
  n = 3
  m = 4
  DIM c, d AS DOUBLE
  c = 1.5
  d = 2.5
  DIM ok AS INTEGER = 1
  IF a <> "x" OR b <> "y" THEN ok = 0
  IF n <> 3 OR m <> 4 THEN ok = 0
  IF c <> 1.5 OR d <> 2.5 THEN ok = 0
  IF ok = 1 THEN
    PRINT "shared_ok"
  ELSE
    PRINT "shared_FAIL"
  END IF
END SUB
