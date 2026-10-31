' MiniTest: STATS.KS — Kolmogorov-Smirnov due campioni (oracolo scipy 1.18).
' EXPECT: d= 0.2|p= 1
IMPORT STATS
IMPORT MATH
SUB Main()
  DIM x = {2.0, 4.0, 5.0, 7.0, 9.0}
  DIM y = {1.0, 3.0, 6.0, 8.0, 10.0}
  DIM r = STATS.KS(x, y)
  PRINT "d="; MATH.ROUND(r.Statistic, 6)
  PRINT "p="; MATH.ROUND(r.PValue, 6)
END SUB
