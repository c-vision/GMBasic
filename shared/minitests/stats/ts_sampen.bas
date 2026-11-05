' EXPECT: sampen= 0.280031
SUB Main()
  DIM x(59) AS DOUBLE
  FOR i = 0 TO 59
    x(i) = SIN(i * 0.2)
  NEXT i
  PRINT "sampen="; MATH.ROUND(TIMESERIES.TSSAMPEN(x, 2, 0.2), 6)
END SUB
