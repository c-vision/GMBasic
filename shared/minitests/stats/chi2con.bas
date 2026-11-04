' MiniTest: STATS.CHI2CON — indipendenza su tabella, SENZA correzione Yates
' (scipy: chi2_contingency(correction=False) = 0.793651/0.372998).
' EXPECT: chi2= 0.793651|p= 0.372998|df= 1
IMPORT STATS
SUB Main()
  DIM tb(1) AS ARRAY OF DOUBLE
  tb(0) = {10, 20}
  tb(1) = {30, 40}
  DIM r = STATS.CHI2CON(tb)
  PRINT "chi2="; MATH.ROUND(r.Statistic, 6)
  PRINT "p="; MATH.ROUND(r.PValue, 6)
  PRINT "df="; r.DegreesOfFreedom
END SUB
