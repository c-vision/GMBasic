' EXPECT: cdf= 0.75|sf= 0.25
SUB Main()
  PRINT "cdf="; MATH.ROUND(STATS.ParetoCdf(2.0, 1, 2), 6)
  PRINT "sf="; MATH.ROUND(STATS.ParetoSf(2.0, 1, 2), 6)
END SUB
