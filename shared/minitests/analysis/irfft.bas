' MiniTest ANALYSIS.IRFFT via FFI (namespace SIGNAL del pacchetto `signal`).
' re = {1, 0.5, 0, 0, 0}, im = 0: ir(0) = (1 + 2*0.5)/8 = 0.25.
' EXPECT: irfft0= 0.25
IMPORT SIGNAL
SUB Main()
  DIM re(4) AS DOUBLE = {1, 0.5, 0, 0, 0}
  DIM im(4) AS DOUBLE = {0, 0, 0, 0, 0}
  DIM ir() = SIGNAL.IRFFT(re, im)
  PRINT "irfft0="; ROUND(ir(0), 6)
END SUB
