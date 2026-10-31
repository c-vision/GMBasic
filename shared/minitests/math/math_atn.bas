' ATN: UN solo comando, 2° argomento opzionale.
'   ATN(y)    = arctan(y)                                   (1 argomento, invariato)
'   ATN(y, x) = atan2(y, x): angolo del vettore (x, y) in [-π, π]
' Non esistono ATN2/ATAN2: la forma a 2 argomenti è ATN stessa.
' EXPECT: atan1 ok|atan2q1 ok|atan2q2 ok|atan2q3 ok|atan2assi ok|atan2neg ok|atan2zero ok|atan2mathns ok|atan0 ok
IMPORT MATH
SUB Main()
  IF ABS(ATN(1) - 0.7853981633974483) < 0.000000000001 THEN PRINT "atan1 ok" ELSE PRINT "atan1 FAIL"
  IF ABS(ATN(1, 1) - 0.7853981633974483) < 0.000000000001 THEN PRINT "atan2q1 ok" ELSE PRINT "atan2q1 FAIL"
  IF ABS(ATN(1, -1) - 2.356194490192345) < 0.000000000001 THEN PRINT "atan2q2 ok" ELSE PRINT "atan2q2 FAIL"
  IF ABS(ATN(-1, -1) + 2.356194490192345) < 0.000000000001 THEN PRINT "atan2q3 ok" ELSE PRINT "atan2q3 FAIL"
  IF ABS(ATN(0, -1) - 3.141592653589793) < 0.000000000001 THEN PRINT "atan2assi ok" ELSE PRINT "atan2assi FAIL"
  IF ABS(ATN(-1, 0) + 1.5707963267948966) < 0.000000000001 THEN PRINT "atan2neg ok" ELSE PRINT "atan2neg FAIL"
  IF ABS(ATN(0, 0)) < 0.000000000001 THEN PRINT "atan2zero ok" ELSE PRINT "atan2zero FAIL"
  IF ABS(MATH.ATN(1, 1) - 0.7853981633974483) < 0.000000000001 THEN PRINT "atan2mathns ok" ELSE PRINT "atan2mathns FAIL"
  IF ABS(ATN(0)) < 0.000000000001 THEN PRINT "atan0 ok" ELSE PRINT "atan0 FAIL"
END SUB
