' MiniTest: STATS.KURTOSISTEST — Anscombe-Glynn preciso (scipy 1.18).
' EXPECT: z=-1.70581|p= 0.088043
IMPORT STATS
SUB Main()
  DIM z = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0, 17.0, 18.0, 19.0, 20.0}
  DIM r = STATS.KURTOSISTEST(z)
  PRINT "z="; MATH.ROUND(r.Statistic, 6)
  PRINT "p="; MATH.ROUND(r.PValue, 6)
END SUB
