' MiniTest: STATS.ZTEST — z-test un campione (formula normale esatta).
' EXPECT: z= 0.447214|p= 0.654721
IMPORT STATS
SUB Main()
  DIM x = {2.0, 4.0, 5.0, 7.0, 9.0}
  DIM r = STATS.ZTEST(x, 5.0, 2.0)
  PRINT "z="; MATH.ROUND(r.Statistic, 6)
  PRINT "p="; MATH.ROUND(r.PValue, 6)
END SUB
