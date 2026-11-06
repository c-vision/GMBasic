' EXPECT: pca= ok
IMPORT MULTIVARIATEX
SUB Main()
  DIM data(3) AS ARRAY OF DOUBLE
  data(0) = {1, 2}
  data(1) = {2, 3}
  data(2) = {3, 4}
  data(3) = {4, 5}
  ' out: autovalori(2) + autovettori(4)
  DIM o(5) AS DOUBLE
  CALL MULTIVARIATEX.PCA(data, o)
  PRINT "pca= ok"
END SUB
