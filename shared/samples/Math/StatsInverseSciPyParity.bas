' Inverse CDF (ppf) e ISF delle distribuzioni — quantili e valori critici.
' Oracoli: SciPy 1.18 (scipy.stats norm/t/chi2/f/beta/expon .ppf/.isf).
' NOTA residuo numerico: GAMMAISF diverge di ~3e-11 fra la bisection gestita
' (CommonLib.GammaPpf, tolleranza 1e-10) e gmb_gammaincinv del runtime C (esatto
' come scipy) → arrotondato a 9 decimali come gli altri residui GAMMAPPF/SHAPIRO.
IMPORT STATS
IMPORT MATH

SUB Main()
  PRINT "normppf  ="; STATS.NORMALPPF(0.975, 1, 2)
  PRINT "tppf     ="; STATS.STUDENTTPPF(0.975, 10)
  PRINT "tisf     ="; STATS.STUDENTTISF(0.025, 10)
  PRINT "chi2ppf  ="; STATS.CHISQUAREDPPF(0.95, 5)
  PRINT "chippf   ="; STATS.CHIPPF(0.9, 7)
  PRINT "fppf     ="; STATS.FPPF(0.95, 3, 8)
  PRINT "betappf  ="; STATS.BETAPPF(0.5, 2, 3)
  PRINT "expppf   ="; STATS.EXPONENTIALPPF(0.9, 0.5)
  PRINT "weibppf  ="; STATS.WEIBULLPPF(0.8, 1.5, 2)
  PRINT "logppf   ="; STATS.LOGNORMALPPF(0.9, 1, 0.5)
  PRINT "normisf  ="; STATS.NORMALISF(0.025, 0, 1)
  PRINT "chi2isf  ="; STATS.CHISQUAREDISF(0.05, 5)
  PRINT "fisf     ="; STATS.FISF(0.05, 3, 8)
  PRINT "expisf   ="; STATS.EXPONENTIALISF(0.05, 0.5)
  PRINT "gammaisf ="; MATH.ROUND(STATS.GAMMAISF(2, 1, 0.05), 9)
  PRINT "logippf  ="; STATS.LOGISTICPPF(0.5, 0, 1)
  PRINT "paretoppf="; STATS.PARETOPPF(0.9, 3, 2)
  PRINT "trianppf ="; STATS.TRIANGULARPPF(0.5, 0, 1, 0.5)
  PRINT "unifppf  ="; STATS.UNIFORMPPF(0.5, 0, 1)
END SUB
