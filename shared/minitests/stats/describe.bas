' MiniTest: STATS.DESCRIBE — descrittive complete. Curtosi = eccesso NON
' distorto (sample, -1.2 su 1..5); scipy.describe riporta -1.3 (distorto).
' EXPECT: n= 5|min= 1|max= 5|mean= 3|var= 2.5|skew= 0|kurt=-1.2
IMPORT STATS
SUB Main()
  DIM v = {1.0, 2.0, 3.0, 4.0, 5.0}
  DIM d = STATS.DESCRIBE(v)
  PRINT "n="; d.Nobs
  PRINT "min="; d.Min
  PRINT "max="; d.Max
  PRINT "mean="; MATH.ROUND(d.Mean, 6)
  PRINT "var="; MATH.ROUND(d.Variance, 6)
  PRINT "skew="; MATH.ROUND(d.Skewness, 6)
  PRINT "kurt="; MATH.ROUND(d.Kurtosis, 6)
END SUB
