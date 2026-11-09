' EXPECT: ev0= 0.24225|ev1= 0.383873|ev2= 0.958431|tr0= 228.64248|tr1= 212.553194|mx0= 16.089286|obs= 58
' TIMESERIES (FFI): Johansen su matrice (righe=osservazioni, colonne=variabili).
IMPORT TIMESERIES
SUB Main()
  DIM y(59, 2) AS DOUBLE
  FOR i = 0 TO 59
    DIM y0 AS DOUBLE = 0.5 * SIN(i * 0.2) + i * 0.01 + 0.4 * SIN(i * 1.7)
    y(i,0) = y0 : y(i,1) = y0 * 0.8 + 0.3 * COS(i * 0.5) + 0.5 * COS(i * 2.3) : y(i,2) = y0 * 1.2 - 0.2 * SIN(i * 0.7) + 0.6 * SIN(i * 1.1)
  NEXT i
  ' out(3 + 3*nvars): nvars, lags, observations, eigenvalues, trace, max
  DIM outj(11) AS DOUBLE
  TIMESERIES.JOHANSEN(y, 1, outj)
  PRINT "ev0="; MATH.ROUND(outj(3), 6)
  PRINT "ev1="; MATH.ROUND(outj(4), 6)
  PRINT "ev2="; MATH.ROUND(outj(5), 6)
  PRINT "tr0="; MATH.ROUND(outj(6), 6)
  PRINT "tr1="; MATH.ROUND(outj(7), 6)
  PRINT "mx0="; MATH.ROUND(outj(9), 6)
  PRINT "obs="; outj(2)
END SUB
