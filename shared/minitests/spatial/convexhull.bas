' MiniTest: SPATIAL.CONVEXHULL (parita' 4 backend) — input MATRIX rank-2.
' EXPECT: 0  1  2  3
IMPORT SPATIAL
SUB Main()
  DIM X(3, 1) AS DOUBLE
  X(0,0) = 0 : X(0,1) = 0
  X(1,0) = 1 : X(1,1) = 0
  X(2,0) = 1 : X(2,1) = 1
  X(3,0) = 0 : X(3,1) = 1
  DIM h() = SPATIAL.CONVEXHULL(X)
  PRINT h(0); h(1); h(2); h(3)
END SUB
