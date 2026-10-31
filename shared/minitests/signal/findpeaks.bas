' MiniTest: SIGNAL.FINDPEAKS (parita' 4 backend).
' EXPECT: 1  3
IMPORT SIGNAL
SUB Main()
  DIM x(6) AS DOUBLE = {1, 3, 2, 5, 4, 6}
  DIM p() = SIGNAL.FINDPEAKS(x, 1)
  PRINT p(0); p(1)
END SUB
