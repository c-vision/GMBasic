' EXPECT: beta= 0.5|chi= 0.317311
SUB Main()
  PRINT "beta="; MATH.ROUND(STATS.BetaSf(0.5, 2, 2), 6)
  PRINT "chi="; MATH.ROUND(STATS.ChiSf(1.0, 1), 6)
END SUB
