' SIGNAL — nucleo scipy.signal (convoluzione/correlazione 1D + finestre).
' Input: x=[1,2,3], k=[0,1,0.5].
' Oracolo: SciPy/numpy 1.18 — convolve=[0,1,2.5,4,1.5], correlate=[0.5,2,3.5,3,0],
' hann(4)=[0,0.75,0.75,0], hamming(4)=[0.08,0.77,0.77,0.08], blackman(4)=[0,0.63,0.63,0].
IMPORT SIGNAL

SUB Main()
  DIM x(2) AS DOUBLE = {1, 2, 3}
  DIM k(2) AS DOUBLE = {0, 1, 0.5}
  DIM c() = SIGNAL.CONVOLVE(x, k)
  DIM r() = SIGNAL.CORRELATE(x, k)
  DIM h() = SIGNAL.HANN(4)
  DIM m() = SIGNAL.HAMMING(4)
  DIM b() = SIGNAL.BLACKMAN(4)
  PRINT "convolve   ="; c(0); c(1); c(2); c(3); c(4)
  PRINT "correlate  ="; r(0); r(1); r(2); r(3); r(4)
  PRINT "hann       ="; h(0); h(1); h(2); h(3)
  PRINT "hamming    ="; m(0); m(1); m(2); m(3)
  PRINT "blackman   ="; b(0); b(1); b(2); b(3)
END SUB
