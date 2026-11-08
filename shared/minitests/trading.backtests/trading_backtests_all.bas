' MiniTest NAMESPACE BACKTEST — RUN (media per periodo, pesi uguali).
' EXPECT: 5.06
IMPORT BACKTEST
SUB Main()
  DIM data(1) AS ARRAY OF DOUBLE
  data(0) = {0.01, 0.03}
  data(1) = {0.02, 0.04}
  DIM r() AS DOUBLE
  r = BACKTEST.RUN(data, "")
  PRINT ROUND(r(0), 8)
END SUB
