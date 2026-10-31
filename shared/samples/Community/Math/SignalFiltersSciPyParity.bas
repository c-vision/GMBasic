' Nucleo filtri IIR (scipy.signal): design Butterworth/Chebyshev/ellittico (handle) +
' LFILTER/FILTFILT/FREQZ. Oracoli: SciPy 1.18 (butter(2,0.3), lfilter, filtfilt, freqz).
IMPORT SIGNAL

SUB Main()
  DIM wn(0) AS DOUBLE = {0.3}
  DIM x(9) AS DOUBLE = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0}

  h = SIGNAL.BUTTER(2, wn, "low")
  DIM y() = SIGNAL.LFILTER(h, x)
  PRINT "lfilter     ="; y(0); y(1); y(2); y(3); y(4); y(5); y(6); y(7); y(8); y(9)
  DIM z() = SIGNAL.FILTFILT(h, x)
  PRINT "filtfilt    ="; z(0); z(1); z(2); z(3); z(4)
  DIM m() = SIGNAL.FREQZ(h, 8)
  PRINT "freqz       ="; m(0); m(1); m(2); m(3); m(4); m(5); m(6); m(7)

  ' Design ellittico (rp=1, rs=20) e applicazione
  h2 = SIGNAL.ELLIP(3, 1.0, 20.0, wn, "low")
  DIM y2() = SIGNAL.LFILTER(h2, x)
  PRINT "ellip lfilt ="; y2(0); y2(1); y2(2); y2(3)

  ' Chebyshev tipo I (rp=1) e tipo II (rs=20)
  h3 = SIGNAL.CHEBY1(2, 1.0, wn, "low")
  DIM y3() = SIGNAL.LFILTER(h3, x)
  PRINT "cheby1 lfilt="; y3(0); y3(1); y3(2); y3(3)
  h4 = SIGNAL.CHEBY2(2, 20.0, wn, "low")
  DIM y4() = SIGNAL.LFILTER(h4, x)
  PRINT "cheby2 lfilt="; y4(0); y4(1); y4(2); y4(3)
END SUB
