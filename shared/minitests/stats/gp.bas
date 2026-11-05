' EXPECT: m0= 0.265013|m1= 0.844469|m2= 0.49602|v0= 0.016212|v1= 0.01569
IMPORT MULTIVARIATE
SUB Main()
  DIM x(9) AS DOUBLE
  DIM y(9) AS DOUBLE
  FOR i = 0 TO 9
    x(i) = i * 0.5
    y(i) = SIN(x(i)) + 0.1 * COS(i * 1.3)
  NEXT i
  DIM xs(4) AS DOUBLE
  xs(0) = 0.2: xs(1) = 1.2: xs(2) = 2.7: xs(3) = 4.1
  DIM r = MULTIVARIATE.GPRREGRESS(x, y, xs, 1.0, 0.01)
  PRINT "m0="; MATH.ROUND(r.Mean(0), 6)
  PRINT "m1="; MATH.ROUND(r.Mean(1), 6)
  PRINT "m2="; MATH.ROUND(r.Mean(2), 6)
  PRINT "v0="; MATH.ROUND(r.Variance(0), 6)
  PRINT "v1="; MATH.ROUND(r.Variance(1), 6)
END SUB
