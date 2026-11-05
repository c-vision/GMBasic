' EXPECT: dstat=-1.874815|dlags= 1|dobs= 58|estat=-262.162522|elags= 1|enreg= 2
' TIMESERIESX (FFI): DF-GLS e Engle-Granger con matrice jagged (righe=osservazioni).
IMPORT TIMESERIESX
SUB Main()
  DIM x(59) AS DOUBLE
  FOR i = 0 TO 59
    x(i) = 0.4 * SIN(i * 0.25) + 0.1 * COS(i * 0.7) + i * 0.03
  NEXT i
  ' out(5): statistic, lags, observations, deterministic, alpha_bar
  DIM outd(4) AS DOUBLE
  TIMESERIESX.DFGLS(x, 1, 0, outd)
  PRINT "dstat="; MATH.ROUND(outd(0), 6)
  PRINT "dlags="; outd(1)
  PRINT "dobs="; outd(2)
  DIM y(49) AS ARRAY OF DOUBLE
  FOR i = 0 TO 49
    DIM y0 AS DOUBLE = i * 0.1 + 0.3 * SIN(i * 0.4)
    y(i) = {y0, y0 * 1.5 + 0.05 * COS(i * 0.9)}
  NEXT i
  ' out(5): statistic, lags, observations, n_regressors, with_constant
  DIM oute(4) AS DOUBLE
  TIMESERIESX.ENGLEGRANGER(y, 0, 1, 1, oute)
  PRINT "estat="; MATH.ROUND(oute(0), 6)
  PRINT "elags="; oute(1)
  PRINT "enreg="; oute(3)
END SUB
