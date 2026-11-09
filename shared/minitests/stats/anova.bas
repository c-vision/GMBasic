' MiniTest: STATS.ANOVA — analisi della varianza a una via (oracolo scipy 1.18).
' EXPECT: f= 4.8|p= 0.070988|df= 6
IMPORT STATS

SUB Main()
  DIM groups(1, 3) AS DOUBLE
  groups = {{2.0, 3.0, 4.0, 5.0}, {4.0, 5.0, 6.0, 7.0}}
  DIM r = STATS.ANOVA(groups)
  PRINT "f="; MATH.ROUND(r.Statistic, 6)
  PRINT "p="; MATH.ROUND(r.PValue, 6)
  PRINT "df="; r.DegreesOfFreedom
END SUB
