' Verifica ordine argomenti CDF/PDF su C99 (era un bug latente: firma C param-first).
' Oracoli: SciPy 1.18.
IMPORT STATS

SUB Main()
  PRINT "ncdf   ="; STATS.NORMALCDF(1.5, 0, 1)
  PRINT "npdf   ="; STATS.NORMALPDF(0, 0, 1)
  PRINT "tcdf   ="; STATS.STUDENTTCDF(2.228138852, 10)
  PRINT "bcdf   ="; STATS.BETACDF(0.5, 2, 3)
  PRINT "c2cdf  ="; STATS.CHISQUAREDCDF(11.070497693516351, 5)
  PRINT "fcdf   ="; STATS.FCDF(4.0661805514, 3, 8)
  PRINT "ecdf   ="; STATS.EXPONENTIALCDF(1.5, 2)
  PRINT "wcdf   ="; STATS.WEIBULLCDF(2.5, 1.5, 2)
  PRINT "lncdf  ="; STATS.LOGNORMALCDF(3, 1, 0.5)
  PRINT "gammacdf="; STATS.GAMMACDF(2, 3, 4)
  PRINT "gammapdf="; STATS.GAMMAPDF(2, 3, 4)
END SUB
