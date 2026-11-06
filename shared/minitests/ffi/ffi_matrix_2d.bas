' Matrice 2D passata a un FFI come buffer piatto row-major (dimensioni esplicite).
' EXPECT: mean= 2.5
DECLARE FUNCTION MEAN2D LIB "gmbstats" ALIAS "gs_stats_mean" (a() AS DOUBLE, n AS LONG) AS DOUBLE
SUB Main()
  DIM m(1, 1) AS DOUBLE
  m(0, 0) = 1: m(0, 1) = 2
  m(1, 0) = 3: m(1, 1) = 4
  PRINT "mean="; MEAN2D(m, 4)
END SUB
