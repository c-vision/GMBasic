' EXPECT: cdf= 0.5|sf= 0.5
IMPORT STATS
SUB Main()
  PRINT "cdf="; MATH.ROUND(STATS.UniformCdf(2.0, 1, 3), 6)
  PRINT "sf="; MATH.ROUND(STATS.UniformSf(2.0, 1, 3), 6)
END SUB
