' MiniTest: STATS.CHI2 — adattamento chi-quadrato (oracolo scipy 1.18).
' EXPECT: chi2= 0.5|p= 0.992123
IMPORT STATS
SUB Main()
  DIM observed = {16.0, 18.0, 16.0, 14.0, 12.0, 12.0}
  DIM expected = {16.0, 16.0, 16.0, 16.0, 12.0, 12.0}
  DIM r = STATS.CHI2(observed, expected)
  PRINT "chi2="; MATH.ROUND(r.Statistic, 6)
  PRINT "p="; MATH.ROUND(r.PValue, 6)
END SUB
