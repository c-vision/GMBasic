' EXPECT: m0= 0.265013|m1= 0.844469|m2= 0.49602|v0= 0.016212|v1= 0.01569
IMPORT MULTIVARIATEX
SUB Main()
  DIM x(9) AS DOUBLE
  DIM y(9) AS DOUBLE
  FOR i = 0 TO 9
    x(i) = i * 0.5
    y(i) = SIN(x(i)) + 0.1 * COS(i * 1.3)
  NEXT i
  DIM xs(3) AS DOUBLE
  xs(0) = 0.2: xs(1) = 1.2: xs(2) = 2.7: xs(3) = 4.1
  ' out: mean(4), var(4)
  DIM o(7) AS DOUBLE
  CALL MULTIVARIATEX.GPRREGRESS(x, y, xs, 1.0, 0.01, o)
  PRINT "m0="; MATH.ROUND(o(0), 6)
  PRINT "m1="; MATH.ROUND(o(1), 6)
  PRINT "m2="; MATH.ROUND(o(2), 6)
  PRINT "v0="; MATH.ROUND(o(4), 6)
  PRINT "v1="; MATH.ROUND(o(5), 6)
END SUB
