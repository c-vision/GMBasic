' EXPECT: jpi0= 0|ja00= 0.893468|jm00= 0.940789
IMPORT TIMESERIES
SUB Main()
  DIM x(19) AS ARRAY OF DOUBLE
  FOR i = 0 TO 19
    x(i) = {SIN(i * 0.35) + i * 0.05, COS(i * 0.35) * 0.8}
  NEXT i
  DIM r = TIMESERIES.HMMFIT(x, 2, 8)
  PRINT "jpi0="; MATH.ROUND(r.Pi(0), 6)
  PRINT "ja00="; MATH.ROUND(r.A(0), 6)
  PRINT "jm00="; MATH.ROUND(r.Means(0), 6)
END SUB
