' EXPECT: splrep_ok= 1
IMPORT ANALYSIS
SUB Main()
  DIM xs(4) AS DOUBLE = {0, 1, 2, 3, 4}
  DIM ys(4) AS DOUBLE = {1, 4, 9, 16, 25}
  DIM sp = ANALYSIS.Splrep(xs, ys)
  PRINT "splrep_ok="; IIF(sp <> 0, 1, 0)
END SUB
