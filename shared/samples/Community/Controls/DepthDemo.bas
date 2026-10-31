' DepthDemo.bas — demo d'applicazione del controllo (Controls/Depth.bas via IMPORT).
IMPORT CustomControls

SUB Main()
  DIM d AS MarketDepth = NEW MarketDepth()
  CONTROL ADD d AT 2, 2 SIZE 30, 12
  d.Configure(10, 1010.0, 1.0)
  d.SetQuote(1003.0, 1004.0)
  DIM rBuy AS INTEGER
  rBuy = d.Hit(5, 6)                    ' 1004 = ask → BUY (lift)
  PRINT "buy row="; rBuy; " side="; d.Side(); " px="; d.LastPrice()
  DIM rSell AS INTEGER
  rSell = d.Hit(5, 7)                   ' 1003 = bid → SELL (hit)
  PRINT "sell row="; rSell; " side="; d.Side()
  d.DragStop(996.0)
  d.DragTarget(1012.0)
  PRINT "sl="; d.Stop(); " tp="; d.Target()
  PRINT "sel="; d.SelectedRow()
  CONTROL REMOVE d
  PRINT "depth-ok"
END SUB
