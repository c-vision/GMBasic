' MiniTest: STATS.MANNWHITNEY — test U, approssimazione normale
' (z=(0-8)/sqrt(12)=-2.3094 -> p=0.020921; scipy esatto=0.028571).
' EXPECT: u= 0|p= 0.020921
IMPORT STATS
IMPORT MATH
SUB Main()
  DIM lo = {1.0, 2.0, 3.0, 4.0}
  DIM hi = {5.0, 6.0, 7.0, 8.0}
  DIM r = STATS.MANNWHITNEY(lo, hi)
  PRINT "u="; MATH.ROUND(r.Statistic, 6)
  PRINT "p="; MATH.ROUND(r.PValue, 6)
END SUB
