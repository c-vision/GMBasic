' EXPECT: dstat= -1.874815|dlags= 1|dobs= 58|estat= -262.162522|elags= 1|enreg= 2
IMPORT TIMESERIES
SUB Main()
  DIM x(59) AS DOUBLE
  FOR i = 0 TO 59
    x(i) = 0.4 * SIN(i * 0.25) + 0.1 * COS(i * 0.7) + i * 0.03
  NEXT i
  DIM d = TIMESERIES.DFGLS(x, 1, 0)
  PRINT "dstat="; MATH.ROUND(d.Statistic, 6)
  PRINT "dlags="; d.Lags
  PRINT "dobs="; d.Observations
  DIM y(49, 1) AS DOUBLE
  FOR i = 0 TO 49
    y(i, 0) = i * 0.1 + 0.3 * SIN(i * 0.4)
    y(i, 1) = y(i, 0) * 1.5 + 0.05 * COS(i * 0.9)
  NEXT i
  DIM e = TIMESERIES.ENGLEGRANGER(y, 0, 1, 1)
  PRINT "estat="; MATH.ROUND(e.Statistic, 6)
  PRINT "elags="; e.Lags
  PRINT "enreg="; e.NRegressors
END SUB
