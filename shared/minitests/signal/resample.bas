' MiniTest: SIGNAL.RESAMPLE/DECIMATE (parita' 4 backend).
' EXPECT: 1.5  1.08578643762691  1.5  2.5  3.5  3.91421356237309  3.5  2.5|0.8  2.4
IMPORT SIGNAL
SUB Main()
  DIM x(3) AS DOUBLE = {1, 2, 3, 4}
  DIM r() = SIGNAL.RESAMPLE(x, 8)
  PRINT r(0); r(1); r(2); r(3); r(4); r(5); r(6); r(7)
  DIM d() = SIGNAL.DECIMATE(x, 2)
  PRINT d(0); d(1)
END SUB
