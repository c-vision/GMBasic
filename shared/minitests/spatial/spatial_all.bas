' MiniTest NAMESPACE SPATIAL — tutte le funzioni (input MATRIX rank-2).
' EXPECT: 1  1|4.472136  4.242641|0  1  2  3|1  4.242641|5  10  5
IMPORT SPATIAL

SUB T_cd()
  DIM X(1, 1) AS DOUBLE
  X(0,0) = 0 : X(0,1) = 0
  X(1,0) = 3 : X(1,1) = 4
  DIM Y(1, 1) AS DOUBLE
  Y(0,0) = 1 : Y(0,1) = 0
  Y(1,0) = 0 : Y(1,1) = 1
  DIM m = SPATIAL.CDIST(X, Y, "euclidean")
  PRINT ROUND(MATRIX.GETVALUE(m, 0, 0), 6); ROUND(MATRIX.GETVALUE(m, 0, 1), 6)
  PRINT ROUND(MATRIX.GETVALUE(m, 1, 0), 6); ROUND(MATRIX.GETVALUE(m, 1, 1), 6)
END SUB

SUB T_ch()
  DIM X(3, 1) AS DOUBLE
  X(0,0) = 0 : X(0,1) = 0
  X(1,0) = 1 : X(1,1) = 0
  X(2,0) = 1 : X(2,1) = 1
  X(3,0) = 0 : X(3,1) = 1
  DIM h() = SPATIAL.CONVEXHULL(X)
  PRINT h(0); h(1); h(2); h(3)
END SUB

SUB T_dm()
  DIM X(1, 1) AS DOUBLE
  X(0,0) = 0 : X(0,1) = 0
  X(1,0) = 3 : X(1,1) = 4
  DIM Y(1, 1) AS DOUBLE
  Y(0,0) = 1 : Y(0,1) = 0
  Y(1,0) = 0 : Y(1,1) = 1
  DIM m = SPATIAL.DISTANCEMATRIX(X, Y)
  PRINT ROUND(MATRIX.GETVALUE(m, 0, 0), 6); ROUND(MATRIX.GETVALUE(m, 1, 1), 6)
END SUB

SUB T_pd()
  DIM X(2, 1) AS DOUBLE
  X(0,0) = 0 : X(0,1) = 0
  X(1,0) = 3 : X(1,1) = 4
  X(2,0) = 6 : X(2,1) = 8
  DIM d() = SPATIAL.PDIST(X, "euclidean")
  PRINT ROUND(d(0), 6); ROUND(d(1), 6); ROUND(d(2), 6)
END SUB

SUB Main()
  T_cd()
  T_ch()
  T_dm()
  T_pd()
END SUB
