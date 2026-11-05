' EXPECT: bpdf= 1.47242|bcdf= 0.413738|bsf= 0.586262|gpdf= 0.261135|gcdf= 0.469853|gsf= 0.530147
SUB Main()
  PRINT "bpdf="; MATH.ROUND(STATS.NcBetaPdf(0.5, 2, 2, 1), 6)
  PRINT "bcdf="; MATH.ROUND(STATS.NcBetaCdf(0.5, 2, 2, 1), 6)
  PRINT "bsf="; MATH.ROUND(STATS.NcBetaSf(0.5, 2, 2, 1), 6)
  PRINT "gpdf="; MATH.ROUND(STATS.NcGammaPdf(2.0, 2, 1), 6)
  PRINT "gcdf="; MATH.ROUND(STATS.NcGammaCdf(2.0, 2, 1), 6)
  PRINT "gsf="; MATH.ROUND(STATS.NcGammaSf(2.0, 2, 1), 6)
END SUB
