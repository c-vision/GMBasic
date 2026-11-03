' MiniTest NAMESPACE TRADING.BACKTESTS — RUN (media per periodo su array 2-D).
' EXPECT: 5.06
IMPORT TRADING.BACKTESTS
SUB Main()
  DIM data(1, 1) AS DOUBLE
  data(0,0)=0.01 : data(0,1)=0.03
  data(1,0)=0.02 : data(1,1)=0.04
  DIM r AS BACKTESTRESULT
  r = TRADING.BACKTESTS.Run(data, "")
  PRINT ROUND(r.TotalReturn, 8)
END SUB
