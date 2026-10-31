' MiniTest: SIGNAL.HANN/HAMMING/BLACKMAN — finestre (parita' 4 backend).
' EXPECT: 0  0.5  1  0.5  0|0.08  0.54  1  0.54  0.08|-1.38777878078145E-17  0.34  1  0.34 -1.38777878078145E-17
IMPORT SIGNAL
SUB Main()
  DIM h() = SIGNAL.HANN(5)
  DIM m() = SIGNAL.HAMMING(5)
  DIM b() = SIGNAL.BLACKMAN(5)
  PRINT h(0); h(1); h(2); h(3); h(4)
  PRINT m(0); m(1); m(2); m(3); m(4)
  PRINT b(0); b(1); b(2); b(3); b(4)
END SUB
