' MiniTest: SPATIAL.CONVEXHULL (parita' 4 backend).
' EXPECT: 0  1  2  3
IMPORT SPATIAL
SUB Main()
  DIM X(3) AS ARRAY OF DOUBLE
  X(0) = {0, 0}
  X(1) = {1, 0}
  X(2) = {1, 1}
  X(3) = {0, 1}
  DIM h() = SPATIAL.CONVEXHULL(X)
  PRINT h(0); h(1); h(2); h(3)
END SUB
