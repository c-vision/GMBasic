' EXPECT: jpi0= 0|ja00= 0.893468|jm00= 0.940789
IMPORT TIMESERIES
SUB Main()
  DIM x(19, 1) AS DOUBLE
  FOR i = 0 TO 19
    x(i,0) = SIN(i * 0.35) + i * 0.05 : x(i,1) = COS(i * 0.35) * 0.8
  NEXT i
  DIM o(17) AS DOUBLE
  CALL TIMESERIES.HMMFIT(x, 2, 8, o)
  PRINT "jpi0="; MATH.ROUND(o(0), 6)
  PRINT "ja00="; MATH.ROUND(o(2), 6)
  PRINT "jm00="; MATH.ROUND(o(6), 6)
END SUB
