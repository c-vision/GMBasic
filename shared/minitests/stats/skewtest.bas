' MiniTest: STATS.SKEWTEST — D'Agostino preciso (scipy 1.18: z=1.040349/p=0.298178
' su 1..20; il campione simmetrico usa y=1, non z=0).
' EXPECT: z= 1.040349|p= 0.298178
IMPORT STATS
IMPORT MATH
SUB Main()
  DIM z = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0, 17.0, 18.0, 19.0, 20.0}
  DIM r = STATS.SKEWTEST(z)
  PRINT "z="; MATH.ROUND(r.Statistic, 6)
  PRINT "p="; MATH.ROUND(r.PValue, 6)
END SUB
