' EXPECT: h= 3.857143|p= 0.049535|df= 1
SUB Main()
  DIM g(1) AS ARRAY OF DOUBLE
  g(0) = {1, 2, 3}
  g(1) = {4, 5, 6}
  DIM r = STATS.Kruskal(g)
  PRINT "h="; MATH.ROUND(r.Statistic, 6)
  PRINT "p="; MATH.ROUND(r.PValue, 6)
  PRINT "df="; r.DegreesOfFreedom
END SUB
