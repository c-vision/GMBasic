' EXPECT: mean= 5.5|median= 5.5|var= 9.166667|sd= 3.02765|p90= 9.1|corr= 1
IMPORT STATSX
SUB Main()
  DIM a(9) AS DOUBLE
  DIM b(9) AS DOUBLE
  FOR i = 0 TO 9
    a(i) = i + 1
    b(i) = 2 * (i + 1) + 0.5
  NEXT i
  PRINT "mean="; MATH.ROUND(STATSX.MEAN(a, 10), 6)
  PRINT "median="; MATH.ROUND(STATSX.MEDIAN(a, 10), 6)
  PRINT "var="; MATH.ROUND(STATSX.VARIANCE(a, 10, 1), 6)
  PRINT "sd="; MATH.ROUND(STATSX.STDDEV(a, 10, 1), 6)
  PRINT "p90="; MATH.ROUND(STATSX.PERCENTILE(a, 10, 90), 6)
  PRINT "corr="; MATH.ROUND(STATSX.CORRELATION(a, b, 10), 6)
END SUB
