' MiniTest: STATS.PEARSONR — correlazione di Pearson (oracolo scipy 1.18).
' EXPECT: r= 1|p= 0
IMPORT STATS
SUB Main()
  DIM x = {1.0, 2.0, 3.0, 4.0, 5.0}
  DIM y = {2.0, 4.0, 6.0, 8.0, 10.0}
  DIM r = STATS.PEARSONR(x, y)
  PRINT "r="; MATH.ROUND(r.Statistic, 6)
  PRINT "p="; MATH.ROUND(r.PValue, 6)
END SUB
