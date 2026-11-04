' EXPECT: ktau= 1|kp= 0.014306|f= 0.061224|fp= 0.115385
SUB Main()
  DIM x() = {1, 2, 3, 4, 5}
  DIM y() = {1, 2, 3, 4, 5}
  DIM k = STATS.KendallTau(x, y)
  PRINT "ktau="; k.Statistic
  PRINT "kp="; MATH.ROUND(k.PValue, 6)
  DIM a() = {1, 2, 3}
  DIM b() = {1, 4, 9}
  DIM f = STATS.FTest(a, b)
  PRINT "f="; MATH.ROUND(f.Statistic, 6)
  PRINT "fp="; MATH.ROUND(f.PValue, 6)
END SUB
