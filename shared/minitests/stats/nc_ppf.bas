' EXPECT: chi2= 12.283754|chi2i= 6.188432|beta= 0.816855|betai= 0.609632|gamma= 6.850496|gammai= 3.679205|f= 5.952706|fi= 2.361365|t= 5.255884|ti= 2.852819
IMPORT STATS
SUB Main()
  PRINT "chi2="; MATH.ROUND(STATS.NcChi2Ppf(0.95, 3, 2), 6)
  PRINT "chi2i="; MATH.ROUND(STATS.NcChi2Isf(0.3, 3, 2), 6)
  PRINT "beta="; MATH.ROUND(STATS.NcBetaPpf(0.95, 2, 3, 2), 6)
  PRINT "betai="; MATH.ROUND(STATS.NcBetaIsf(0.3, 2, 3, 2), 6)
  PRINT "gamma="; MATH.ROUND(STATS.NcGammaPpf(0.95, 2, 2), 6)
  PRINT "gammai="; MATH.ROUND(STATS.NcGammaIsf(0.3, 2, 2), 6)
  PRINT "f="; MATH.ROUND(STATS.NcFPpf(0.95, 3, 10, 2), 6)
  PRINT "fi="; MATH.ROUND(STATS.NcFIsf(0.3, 3, 10, 2), 6)
  PRINT "t="; MATH.ROUND(STATS.NcTPpf(0.95, 5, 2), 6)
  PRINT "ti="; MATH.ROUND(STATS.NcTIsf(0.3, 5, 2), 6)
END SUB
