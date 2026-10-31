' EXPECT: irfft0= 0.25
IMPORT ANALYSIS
SUB Main()
  DIM re(4) AS DOUBLE = {1, 0.5, 0, 0, 0}
  DIM im(4) AS DOUBLE = {0, 0, 0, 0, 0}
  DIM ir() = ANALYSIS.Irfft(re, im)
  PRINT "irfft0="; ROUND(ir(0), 6)
END SUB