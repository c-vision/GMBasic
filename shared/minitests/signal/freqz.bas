' MiniTest: SIGNAL.BUTTER + FREQZ (parita' 4 backend).
' EXPECT: 1  0.834275  0.251286  0.044499
IMPORT SIGNAL
SUB Main()
  DIM wn(0) AS DOUBLE = {0.3}
  DIM h = SIGNAL.BUTTER(2, wn, "low")
  DIM f() = SIGNAL.FREQZ(h, 4)
  PRINT ROUND(f(0), 6); ROUND(f(1), 6); ROUND(f(2), 6); ROUND(f(3), 6)
END SUB
