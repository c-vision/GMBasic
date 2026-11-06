IMPORT TIMESERIES
SUB Main()
  DIM y(3) AS DOUBLE = {0, 1, 4, 9}
  DIM x(3) AS DOUBLE = {0, 1, 2, 3}
  DIM ct() = NUMERICAL.CumTrapezoid(y, x)
  PRINT ct(0); ct(1); ct(2)
  DIM f6() = NUMERICAL.FftFreq(6, 1)
  PRINT f6(0); f6(1); f6(2); f6(3); f6(4); f6(5)
  DIM fs() = NUMERICAL.FftShift(x)
  PRINT fs(0); fs(1); fs(2); fs(3)
  DIM ifs() = NUMERICAL.IfftShift(x)
  PRINT ifs(0); ifs(1); ifs(2); ifs(3)
  DIM yw() = TIMESERIES.YuleWalker(y, 2)
  PRINT yw(0); yw(1)
  DIM mc() = TIMESERIES.MaCoefficients(x, 2)
  PRINT mc(0); mc(1)
END SUB
