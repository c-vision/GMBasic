' EXPECT: weib= 0.832555|uni= 0.5|par= 1.414214
SUB Main()
  PRINT "weib="; MATH.ROUND(STATS.WeibullIsf(0.5, 2, 1), 6)
  PRINT "uni="; MATH.ROUND(STATS.UniformIsf(0.5, 0, 1), 6)
  PRINT "par="; MATH.ROUND(STATS.ParetoIsf(0.5, 1, 2), 6)
END SUB
