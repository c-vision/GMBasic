' EXPECT: F= 4.8
IMPORT STATS
SUB Main()
  DIM g(1, 3) AS DOUBLE
  g = {{2.0, 3.0, 4.0, 5.0}, {4.0, 5.0, 6.0, 7.0}}
  DIM aov AS STATS.AnovaResult
  aov = STATS.Anova(g)
  PRINT "F="; MATH.ROUND(aov.FValue, 4)
END SUB
