' MiniTest: METRICS.CALMAR — parita' 4 backend.
' EXPECT: 24.20272006
IMPORT METRICS AS TM

SUB Main
  DIM r(4) AS DOUBLE : r(0)=0.01 : r(1)=-0.005 : r(2)=0.02 : r(3)=0.015 : r(4)=0.008
  PRINT ROUND(TM.CalmarRatio(r, 12), 8)
END SUB
