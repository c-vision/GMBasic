' MiniTest: STATS.TTEST — t-test indipendente (oracolo scipy 1.18).
' EXPECT: t=-0.098533|p= 0.923933
IMPORT STATS
IMPORT MATH
SUB Main()
  DIM x = {2.0, 4.0, 5.0, 7.0, 9.0}
  DIM y = {1.0, 3.0, 6.0, 8.0, 10.0}
  DIM r = STATS.TTEST(x, y)
  PRINT "t="; MATH.ROUND(r.Statistic, 6)
  PRINT "p="; MATH.ROUND(r.PValue, 6)
END SUB
