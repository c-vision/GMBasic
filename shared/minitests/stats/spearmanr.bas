' MiniTest: STATS.SPEARMANR — correlazione per ranghi (oracolo scipy 1.18).
' EXPECT: rho= 1|p= 0
IMPORT STATS
IMPORT MATH
SUB Main()
  DIM x = {1.0, 2.0, 3.0, 4.0, 5.0}
  DIM y = {2.0, 4.0, 6.0, 8.0, 10.0}
  DIM r = STATS.SPEARMANR(x, y)
  PRINT "rho="; MATH.ROUND(r.Statistic, 6)
  PRINT "p="; MATH.ROUND(r.PValue, 6)
END SUB
