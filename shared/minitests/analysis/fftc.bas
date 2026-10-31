IMPORT ANALYSIS
SUB Main()
  DIM x(3) AS DOUBLE = {1, 0, -1, 0}
  DIM imz(3) AS DOUBLE = {0, 0, 0, 0}
  DIM f() = ANALYSIS.FFTCOMPLEX(x, imz)
  PRINT f(0,0); f(0,1); f(1,0); f(1,1)
  DIM r() = ANALYSIS.IFFTCOMPLEX(x, imz)
  PRINT r(0,0); r(1,0); r(2,0); r(3,0)
END SUB
