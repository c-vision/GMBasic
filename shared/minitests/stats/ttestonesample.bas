' MiniTest: STATS.TTESTONESAMPLE — t-test un campione (oracolo scipy 1.18).
' EXPECT: t= 0.331042|p= 0.757228
IMPORT STATS
IMPORT MATH
SUB Main()
  DIM x = {2.0, 4.0, 5.0, 7.0, 9.0}
  DIM r = STATS.TTESTONESAMPLE(x, 5.0)
  PRINT "t="; MATH.ROUND(r.Statistic, 6)
  PRINT "p="; MATH.ROUND(r.PValue, 6)
END SUB
