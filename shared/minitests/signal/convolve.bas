' MiniTest: SIGNAL.CONVOLVE/CORRELATE (parita' 4 backend).
' EXPECT: 1  3  5  7  4|1  3  5  7  4
IMPORT SIGNAL
SUB Main()
  DIM x(3) AS DOUBLE = {1, 2, 3, 4}
  DIM k(1) AS DOUBLE = {1, 1}
  DIM c() = SIGNAL.CONVOLVE(x, k)
  PRINT c(0); c(1); c(2); c(3); c(4)
  DIM r() = SIGNAL.CORRELATE(x, k)
  PRINT r(0); r(1); r(2); r(3); r(4)
END SUB
