' EXPECT: beta= 0.5|logi= 0|logn= 1|tri= 0.5
IMPORT STATS
SUB Main()
  PRINT "beta="; MATH.ROUND(STATS.BetaIsf(0.5, 2, 2), 6)
  PRINT "logi="; MATH.ROUND(STATS.LogisticIsf(0.5, 0, 1), 6)
  PRINT "logn="; MATH.ROUND(STATS.LognormalIsf(0.5, 0, 1), 6)
  PRINT "tri="; MATH.ROUND(STATS.TriangularIsf(0.5, 0, 0.5, 1), 6)
END SUB
