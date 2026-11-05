' EXPECT: f0= 1|f4= 2.460938
SUB Main()
  DIM x() = {1, 2, 3, 4, 5}
  DIM y() = TIMESERIES.TSFRACDIFF(x, 0.5)
  PRINT "f0="; MATH.ROUND(y(0), 6)
  PRINT "f4="; MATH.ROUND(y(4), 6)
END SUB
