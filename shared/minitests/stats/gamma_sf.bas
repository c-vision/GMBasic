' EXPECT: sf= 0.199148|cdf+sf= 1
SUB Main()
  DIM x = 3.0
  DIM sf = STATS.GammaSf(2, 1, x)
  PRINT "sf="; MATH.ROUND(sf, 6)
  PRINT "cdf+sf="; MATH.ROUND(STATS.GammaCdf(2, 1, x) + sf, 6)
END SUB
