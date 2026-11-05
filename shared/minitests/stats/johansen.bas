' EXPECT: ev0= 0.24225|ev1= 0.383873|ev2= 0.958431|tr0= 228.64248|tr1= 212.553194|mx0= 16.089286|obs= 58
IMPORT TIMESERIES
SUB Main()
  DIM y(59, 2) AS DOUBLE
  FOR i = 0 TO 59
    y(i, 0) = 0.5 * SIN(i * 0.2) + i * 0.01 + 0.4 * SIN(i * 1.7)
    y(i, 1) = y(i, 0) * 0.8 + 0.3 * COS(i * 0.5) + 0.5 * COS(i * 2.3)
    y(i, 2) = y(i, 0) * 1.2 - 0.2 * SIN(i * 0.7) + 0.6 * SIN(i * 1.1)
  NEXT i
  DIM j = TIMESERIES.JOHANSEN(y, 1)
  PRINT "ev0="; MATH.ROUND(j.EigenValues(0), 6)
  PRINT "ev1="; MATH.ROUND(j.EigenValues(1), 6)
  PRINT "ev2="; MATH.ROUND(j.EigenValues(2), 6)
  PRINT "tr0="; MATH.ROUND(j.TraceStatistic(0), 6)
  PRINT "tr1="; MATH.ROUND(j.TraceStatistic(1), 6)
  PRINT "mx0="; MATH.ROUND(j.MaxStatistic(0), 6)
  PRINT "obs="; j.Observations
END SUB
