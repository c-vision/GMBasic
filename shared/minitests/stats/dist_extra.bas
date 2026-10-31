' EXPECT: gcdf= 0.800852|gpdf= 0.149361|gppf= 1.678347|gisf= 1.678347|iqr= 2|mode= 1|moment= 2
IMPORT STATS
IMPORT MATH
SUB Main()
  PRINT "gcdf="; MATH.ROUND(STATS.GammaCdf(2, 1, 3), 6)
  PRINT "gpdf="; MATH.ROUND(STATS.GammaPdf(2, 1, 3), 6)
  PRINT "gppf="; MATH.ROUND(STATS.GammaPpf(2, 1, 0.5), 6)
  PRINT "gisf="; MATH.ROUND(STATS.GammaIsf(2, 1, 0.5), 6)
  DIM a = {1, 2, 3, 4, 5}
  PRINT "iqr="; STATS.Iqr(a)
  PRINT "mode="; STATS.Mode(a)
  PRINT "moment="; STATS.Moment(a, 2)
END SUB
