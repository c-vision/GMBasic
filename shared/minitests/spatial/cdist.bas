' MiniTest: SPATIAL.CDIST (parita' 4 backend) — input MATRIX rank-2.
' EXPECT: 1  1|4.472136  4.242641
IMPORT SPATIAL
SUB Main()
  DIM X(1, 1) AS DOUBLE
  X(0,0) = 0 : X(0,1) = 0
  X(1,0) = 3 : X(1,1) = 4
  DIM Y(1, 1) AS DOUBLE
  Y(0,0) = 1 : Y(0,1) = 0
  Y(1,0) = 0 : Y(1,1) = 1
  m = SPATIAL.CDIST(X, Y, "euclidean")
  PRINT ROUND(MATRIX.GETVALUE(m, 0, 0), 6); ROUND(MATRIX.GETVALUE(m, 0, 1), 6)
  PRINT ROUND(MATRIX.GETVALUE(m, 1, 0), 6); ROUND(MATRIX.GETVALUE(m, 1, 1), 6)
END SUB
