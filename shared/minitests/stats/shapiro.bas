' MiniTest: STATS.SHAPIRO — Shapiro-Wilk (oracolo scipy 1.18).
' EXPECT: w= 0.986339|p= 0.990059
IMPORT STATS
SUB Main()
  DIM normal = {0.12, -0.31, 0.45, 1.02, -0.76, 0.33, 0.18, -0.22, 0.57, -0.41}
  DIM r = STATS.SHAPIRO(normal)
  PRINT "w="; MATH.ROUND(r.Statistic, 6)
  PRINT "p="; MATH.ROUND(r.PValue, 6)
END SUB
