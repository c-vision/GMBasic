' MiniTest: STATS.TTESTPAIRED — t-test appaiato (oracolo scipy 1.18).
' EXPECT: t= 1.5|p= 0.208
IMPORT STATS
IMPORT MATH
SUB Main()
  DIM x = {2.0, 4.0, 5.0, 7.0, 9.0}
  DIM paired = {1.5, 3.5, 5.5, 6.5, 8.5}
  DIM r = STATS.TTESTPAIRED(x, paired)
  PRINT "t="; MATH.ROUND(r.Statistic, 6)
  PRINT "p="; MATH.ROUND(r.PValue, 6)
END SUB
