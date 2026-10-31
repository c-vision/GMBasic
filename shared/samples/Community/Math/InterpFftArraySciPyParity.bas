' Funzioni ANALYSIS array-based (scipy.integrate/fft) verificabili anche su C99
' (senza closure: quelle sono gap C99 noto). Oracoli: SciPy 1.18.
IMPORT ANALYSIS

SUB Main()
  DIM yy(3) AS DOUBLE = {0, 1, 4, 9}
  DIM xx(3) AS DOUBLE = {0, 1, 2, 3}
  DIM yy2(3) AS DOUBLE = {0, 0.5, 2, 1.5}

  PRINT "trapezoid   ="; ANALYSIS.TRAPEZOID(yy, xx)
  DIM ct() = ANALYSIS.CUMTRAPEZOID(yy, xx)
  PRINT "cumtrap     ="; ct(0); ct(1); ct(2)
  PRINT "cubicspline ="; ANALYSIS.CUBICSPLINE(xx, yy2, 1.5)

  DIM f6() = ANALYSIS.FFTFREQ(6, 1)
  PRINT "fftfreq6    ="; f6(0); f6(1); f6(2); f6(3); f6(4); f6(5)
  DIM a6(5) AS DOUBLE = {0, 1, 2, 3, 4, 5}
  DIM fs() = ANALYSIS.FFTSHIFT(a6)
  PRINT "fftshift    ="; fs(0); fs(1); fs(2); fs(3); fs(4); fs(5)
  DIM ifs() = ANALYSIS.IFFTSHIFT(a6)
  PRINT "ifftshift   ="; ifs(0); ifs(1); ifs(2); ifs(3); ifs(4); ifs(5)
END SUB
