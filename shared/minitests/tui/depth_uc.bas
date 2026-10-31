' Depth.bas — Market Depth / DOM (best bid/ask) come UserControl (stile VB6), in BASIC.
IMPORT CONTROLS

CLASS MarketDepth
  PRIVATE DIM mBidPx AS DOUBLE
  PRIVATE DIM mBidQty AS DOUBLE
  PRIVATE DIM mAskPx AS DOUBLE
  PRIVATE DIM mAskQty AS DOUBLE

  SUB SetQuote(bid AS DOUBLE, bq AS DOUBLE, ask AS DOUBLE, aq AS DOUBLE)
    mBidPx = bid
    mBidQty = bq
    mAskPx = ask
    mAskQty = aq
  END SUB

  SUB Paint(g AS ANY)
    CONTROL DRAW g, 0                    ' clear
    CONTROL DRAW g, 1, 65280             ' verde: barra bid
    CONTROL DRAW g, 4, 0, 0, mBidQty, 2
    CONTROL DRAW g, 1, 16711680          ' rosso: barra ask
    CONTROL DRAW g, 4, 0, 4, mAskQty, 2
  END SUB
END CLASS

SUB Main()
  DIM d AS MarketDepth = NEW MarketDepth()
  d.SetQuote(1000, 8, 1001, 6)
  CONTROL ADD d AT 2, 2 SIZE 40, 20
  PRINT "OK-depth-uc"
END SUB
