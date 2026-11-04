IMPORT MULTIVARIATE
IMPORT REGRESSION
SUB Main()
  DIM x(3) AS DOUBLE = {1, 2, 3, 4}
  DIM y(3) AS DOUBLE = {2.1, 4.2, 6.1, 8.0}
  DIM r AS REGRESSION.RegressionResult
  r = REGRESSION.Regress(x, y)
  PRINT r.Slope
  PRINT r.Intercept
  PRINT r.R2
  DIM d(4) AS DOUBLE = {1, 2, 10, 11}
  DIM k AS MULTIVARIATE.KMeansResult
  k = MULTIVARIATE.KMeans(d, 2)
  PRINT k.Centroids(0)
  PRINT k.Centroids(1)
END SUB
