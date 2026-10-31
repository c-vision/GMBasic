' Minitest MarketDepth/DOM UserControl (§12.4) — ladder + click-to-trade + drag SL/TP, parità 4 backend.
' EXPECT: depth-ok
IMPORT CONTROLS
IMPORT CustomControls

SUB Main()
  DIM d AS MarketDepth = NEW MarketDepth()
  CONTROL ADD d AT 2, 2 SIZE 30, 12
  d.Configure(10, 1010.0, 1.0)
  d.SetQuote(1003.0, 1004.0)
  DIM rBuy AS INTEGER
  rBuy = d.Hit(5, 6)                    ' 1004 = ask → BUY (lift)
  DIM sBuy AS INTEGER
  sBuy = d.Side()
  DIM pBuy AS DOUBLE
  pBuy = d.LastPrice()
  DIM rSell AS INTEGER
  rSell = d.Hit(5, 7)                   ' 1003 = bid → SELL (hit)
  DIM sSell AS INTEGER
  sSell = d.Side()
  DIM rOut AS INTEGER
  rOut = d.Hit(5, 99)                   ' fuori dal ladder
  d.DragStop(996.0)
  d.DragTarget(1012.0)
  DIM rowOk AS INTEGER
  rowOk = d.RowAt(1008.0)              ' (1010-1008)/1 = 2
  CONTROL REMOVE d
  IF rBuy = 6 AND sBuy = 0 AND ABS(pBuy - 1004.0) < 0.001 AND rSell = 7 AND sSell = 1 AND rOut = -1 AND ABS(d.Stop() - 996.0) < 0.001 AND ABS(d.Target() - 1012.0) < 0.001 AND d.SelectedRow() = 7 AND rowOk = 2 AND d.PriceAt(0) = 1010.0 THEN
    PRINT "depth-ok"
  ELSE
    PRINT "depth-no"
  END IF
END SUB
