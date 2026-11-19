' EXPECT: 1.96|0.200000000000001|0.999375650364204|1|10.5
IMPORT STATS
IMPORT MULTIVARIATE
IMPORT MULTIVARIATE
SUB Main()
  DIM x(3) AS DOUBLE = {1, 2, 3, 4}
  DIM y(3) AS DOUBLE = {2.1, 4.2, 6.1, 8.0}
  DIM r AS STATS.RegressionResult
  r = STATS.Regress(x, y)
  PRINT r.Slope
  PRINT r.Intercept
  PRINT r.R2
  DIM d(4) AS DOUBLE = {1, 2, 10, 11}
  ' out() = centri(k) + assegnazioni(n): k=2, n=5 → servono 7 slot (0..6).
  ' Con ko(5) (6 slot) la lib scriveva fuori dall'array → corruzione dello heap
  ' (crash intermittente a fine esecuzione).
  DIM ko(6) AS DOUBLE
  CALL MULTIVARIATE.KMEANS(d, 2, ko)
  PRINT ko(0)
  PRINT ko(1)
END SUB
