' MiniTest: SPATIAL.PDIST (parita' 4 backend) — input MATRIX rank-2.
' EXPECT: 5  10  5
IMPORT SPATIAL
SUB Main()
  DIM X(2, 1) AS DOUBLE
  X(0,0) = 0 : X(0,1) = 0
  X(1,0) = 3 : X(1,1) = 4
  X(2,0) = 6 : X(2,1) = 8
  DIM d() = SPATIAL.PDIST(X, "euclidean")
  PRINT ROUND(d(0), 6); ROUND(d(1), 6); ROUND(d(2), 6)
END SUB
