' EXPECT: d= 6
SUB Main()
  DIM d = NUMERICAL.Differentiate(FN(x AS DOUBLE) AS DOUBLE => x * x, 3.0)
  PRINT "d="; MATH.ROUND(d, 6)
END SUB
