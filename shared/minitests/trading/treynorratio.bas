' MiniTest: TRADING.METRICS.TREYNORRATIO — parita' 4 backend.
' EXPECT: -0.00415436
IMPORT TRADING.METRICS AS TM

SUB Main
  DIM r(4) AS DOUBLE : r(0)=0.01 : r(1)=-0.005 : r(2)=0.02 : r(3)=0.015 : r(4)=0.008
  DIM b(4) AS DOUBLE : b(0)=0.008 : b(1)=-0.003 : b(2)=0.015 : b(3)=0.01 : b(4)=0.005
  PRINT ROUND(TM.TreynorRatio(r, b, 0.01, 12), 8)
END SUB
