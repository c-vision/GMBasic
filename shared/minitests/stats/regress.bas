' MiniTest: STATS.REGRESS — regressione lineare ONE-PASS (ex REGRESSION.Regress,
' namespace REGRESSION eliminato). Dati perfettamente lineari y = 2x + 1.
' Distinta da STATS.LINREGRESS (scipy, due passate: restituisce anche R/PValue/Stderr).
' EXPECT: slope= 2|intercept= 1|r2= 1
IMPORT STATS
SUB Main()
  DIM x = {1.0, 2.0, 3.0, 4.0, 5.0}
  DIM y = {3.0, 5.0, 7.0, 9.0, 11.0}
  DIM r AS STATS.RegressionResult
  r = STATS.REGRESS(x, y)
  PRINT "slope="; MATH.ROUND(r.Slope, 6)
  PRINT "intercept="; MATH.ROUND(r.Intercept, 6)
  PRINT "r2="; MATH.ROUND(r.R2, 6)
END SUB
