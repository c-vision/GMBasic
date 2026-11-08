' MiniTest NAMESPACE BACKTEST — SIMULATE su matrice 2-D (media per periodo).
' EXPECT: 5.06
IMPORT BACKTEST
SUB Main()
  DIM data(1, 1) AS DOUBLE
  data(0,0)=0.01 : data(0,1)=0.03
  data(1,0)=0.02 : data(1,1)=0.04
  DIM o(7) AS DOUBLE
  DIM k AS LONG
  k = BACKTEST.SIMULATE(data, o)
  PRINT ROUND(o(0), 8)
END SUB
