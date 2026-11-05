' EXPECT: omega= 6.04E-06|alpha= 0.1|beta= 0.8
SUB Main()
  DIM r(49) AS DOUBLE
  FOR i = 0 TO 49
    r(i) = 0.01 * SIN(i * 0.7) + 0.005 * COS(i * 1.3)
  NEXT i
  DIM g = TIMESERIES.TSGARCH11(r)
  PRINT "omega="; MATH.ROUND(g.Omega, 8)
  PRINT "alpha="; MATH.ROUND(g.Alpha, 6)
  PRINT "beta="; MATH.ROUND(g.Beta, 6)
END SUB
