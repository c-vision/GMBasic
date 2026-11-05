' EXPECT: cdf= 0.666667|sf= 0.333333
SUB Main()
  PRINT "cdf="; MATH.ROUND(STATS.TriangularCdf(2.0, 0, 1, 4), 6)
  PRINT "sf="; MATH.ROUND(STATS.TriangularSf(2.0, 0, 1, 4), 6)
END SUB
