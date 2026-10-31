' MiniTest: SIGNAL.FIRWIN (parita' 4 backend).
' EXPECT: 0  0.151365345728131  0.6  0.151365345728131  0
IMPORT SIGNAL
SUB Main()
  DIM c(0) AS DOUBLE = {0.3}
  DIM w() = SIGNAL.FIRWIN(5, c, "lowpass", "hann")
  PRINT w(0); w(1); w(2); w(3); w(4)
END SUB
