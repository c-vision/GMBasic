' EXPECT: ok=3-1-2
IMPORT ARRAYS
SUB Main
  DIM a = {3, 1, 2}
  DIM j AS STRING
  j = ARRAYS.AJOIN(a, "-")
  PRINT "ok="; j
END SUB
