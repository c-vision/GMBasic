' EXPECT: aeval-ok
IMPORT ARRAYS
SUB Main()
  DIM a = {1, 2, 3}
  ARRAYS.AEVAL(a, SUB(x) { IF x = 2 THEN PRINT "aeval-ok" })
END SUB
