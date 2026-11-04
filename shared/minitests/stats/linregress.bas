' MiniTest: STATS.LINREGRESS — regressione OLS (oracolo scipy 1.18).
' Dati NON degeneri (perfetta collinearita' => t indefinito: p=1 ovunque).
' EXPECT: slope= 1.99|intercept= 0.05|r= 0.998652|p= 5.9E-05|se= 0.059722
IMPORT STATS
SUB Main()
  DIM x = {1.0, 2.0, 3.0, 4.0, 5.0}
  DIM y = {2.1, 3.9, 6.2, 7.8, 10.1}
  DIM r = STATS.LINREGRESS(x, y)
  PRINT "slope="; MATH.ROUND(r.Slope, 6)
  PRINT "intercept="; MATH.ROUND(r.Intercept, 6)
  PRINT "r="; MATH.ROUND(r.R, 6)
  PRINT "p="; MATH.ROUND(r.PValue, 6)
  PRINT "se="; MATH.ROUND(r.Stderr, 6)
END SUB
