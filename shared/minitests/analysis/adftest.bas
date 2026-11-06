' EXPECT: adf_stat= 0  adf_p= 0.75
IMPORT TIMESERIES
SUB Main()
  DIM d(49) AS DOUBLE
  FOR i = 0 TO 49: d(i) = i: NEXT
  DIM r() AS DOUBLE = TIMESERIES.ADFTEST(d)
  PRINT "adf_stat="; ROUND(r(0), 6); " adf_p="; ROUND(r(1), 6)
END SUB
