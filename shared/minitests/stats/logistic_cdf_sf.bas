' EXPECT: cdf= 0.731059|sf= 0.268941
IMPORT STATS
SUB Main()
  PRINT "cdf="; MATH.ROUND(STATS.LogisticCdf(1.0, 0, 1), 6)
  PRINT "sf="; MATH.ROUND(STATS.LogisticSf(1.0, 0, 1), 6)
END SUB
