' EXPECT: rfft_ok= 1
IMPORT ANALYSIS
SUB Main()
  DIM x(7) AS DOUBLE
  FOR i = 0 TO 7: x(i) = COS(2 * 3.141592653589793 * i / 8): NEXT
  DIM rf = ANALYSIS.Rfft(x)
  PRINT "rfft_ok="; IIF(rf <> 0, 1, 0)
END SUB