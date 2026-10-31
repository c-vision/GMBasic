' MiniTest: SPATIAL.PDIST (parita' 4 backend).
' EXPECT: 5  10  5
IMPORT SPATIAL
SUB Main()
  DIM X(2) AS ARRAY OF DOUBLE
  X(0) = {0, 0}
  X(1) = {3, 4}
  X(2) = {6, 8}
  DIM d() = SPATIAL.PDIST(X, "euclidean")
  PRINT ROUND(d(0), 6); ROUND(d(1), 6); ROUND(d(2), 6)
END SUB
