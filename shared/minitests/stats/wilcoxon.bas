' MiniTest: STATS.WILCOXON — ranghi con segno, approssimazione normale
' (W=6, z=(6-7.5)/sqrt(13.75) -> p=0.68583; scipy esatto=1.0).
' EXPECT: w= 6|p= 0.68583
IMPORT STATS
SUB Main()
  DIM a = {1.0, 2.0, 3.0, 4.0, 5.0}
  DIM b = {2.0, 1.0, 4.0, 3.0, 6.0}
  DIM r = STATS.WILCOXON(a, b)
  PRINT "w="; MATH.ROUND(r.Statistic, 6)
  PRINT "p="; MATH.ROUND(r.PValue, 6)
END SUB
