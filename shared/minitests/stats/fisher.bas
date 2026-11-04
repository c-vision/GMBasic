' MiniTest: STATS.FISHER — test esatto di Fisher (oracolo scipy 1.18).
' EXPECT: oddsratio= 20|p= 0.034965
IMPORT STATS
SUB Main()
  DIM r = STATS.FISHER(8, 2, 1, 5)
  PRINT "oddsratio="; MATH.ROUND(r.Statistic, 6)
  PRINT "p="; MATH.ROUND(r.PValue, 6)
END SUB
