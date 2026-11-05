' EXPECT: theta= 4.3E-05|mu= 301.674413|phi= 0.999957
SUB Main()
  DIM x(39) AS DOUBLE
  FOR i = 0 TO 39
    x(i) = 1.0 + 0.5 * SIN(i * 0.2)
  NEXT i
  DIM r = TIMESERIES.TSOUFIT(x, 1.0)
  PRINT "theta="; MATH.ROUND(r.Theta, 6)
  PRINT "mu="; MATH.ROUND(r.Mu, 6)
  PRINT "phi="; MATH.ROUND(r.Phi, 6)
END SUB
