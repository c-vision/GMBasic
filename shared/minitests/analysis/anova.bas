' EXPECT: F= 4.8
IMPORT ANALYSIS
IMPORT MATH
SUB Main()
  DIM g(1) AS ARRAY OF DOUBLE
  g(0) = {2.0, 3.0, 4.0, 5.0}
  g(1) = {4.0, 5.0, 6.0, 7.0}
  DIM aov AS ANALYSIS.AnovaResult
  aov = ANALYSIS.ANOVA(g)
  PRINT "F="; MATH.ROUND(aov.FValue, 4)
END SUB
