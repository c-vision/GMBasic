' EXPECT: mu0= 1.016354|mu1= 5.04487|v0= 0.019728|w0= 0.666667|w1= 0.333333
IMPORT MULTIVARIATE
SUB Main()
  DIM x(29) AS DOUBLE
  FOR i = 0 TO 19
    x(i) = 1.0 + 0.2 * SIN(i * 1.1)
  NEXT i
  FOR i = 20 TO 29
    x(i) = 5.0 + 0.2 * COS(i * 0.9)
  NEXT i
  DIM g = MULTIVARIATE.GMMEM1D(x, 2, 50)
  PRINT "mu0="; MATH.ROUND(g.Mu(0), 6)
  PRINT "mu1="; MATH.ROUND(g.Mu(1), 6)
  PRINT "v0="; MATH.ROUND(g.Variance(0), 6)
  PRINT "w0="; MATH.ROUND(g.Weight(0), 6)
  PRINT "w1="; MATH.ROUND(g.Weight(1), 6)
END SUB
