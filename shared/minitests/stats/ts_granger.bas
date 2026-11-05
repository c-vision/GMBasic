' EXPECT: F= 1.569942|p= 0.223202|df= 2
SUB Main()
  DIM x(39) AS DOUBLE
  DIM y(39) AS DOUBLE
  FOR i = 0 TO 39
    x(i) = SIN(i * 0.3) + 0.01 * i + 0.2 * SIN(i * 1.7)
    y(i) = SIN((i - 1) * 0.3) + 0.3 * COS(i * 2.1)
  NEXT i
  DIM r = TIMESERIES.TSGRANGER(x, y, 2)
  PRINT "F="; MATH.ROUND(r.Statistic, 6)
  PRINT "p="; MATH.ROUND(r.PValue, 6)
  PRINT "df="; r.DegreesOfFreedom
END SUB
