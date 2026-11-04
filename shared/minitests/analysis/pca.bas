' EXPECT: pca= ok
IMPORT MULTIVARIATE
SUB Main()
  DIM data(3, 1) AS DOUBLE
  data(0,0) = 1: data(0,1) = 2
  data(1,0) = 2: data(1,1) = 3
  data(2,0) = 3: data(2,1) = 4
  data(3,0) = 4: data(3,1) = 5
  DIM r AS MULTIVARIATE.PcaResult
  r = MULTIVARIATE.Pca(data)
  PRINT "pca= ok"
END SUB