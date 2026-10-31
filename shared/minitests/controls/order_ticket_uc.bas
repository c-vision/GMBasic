' Minitest OrderTicket UserControl (§12.4) — stato + metodo Hit + accessor, parità 4 backend.
' EXPECT: orderticket-ok
IMPORT CONTROLS
IMPORT CustomControls

SUB Main()
  DIM t AS OrderTicket = NEW OrderTicket()
  CONTROL ADD t AT 2, 2 SIZE 38, 12
  t.SetOrigin(2, 2)
  t.SetQty(5)
  t.SetPrice(123.5)
  DIM rBuy AS INTEGER
  rBuy = t.Hit(6, 9)      ' BUY
  DIM sideBuy AS INTEGER
  sideBuy = t.Side()
  DIM rSell AS INTEGER
  rSell = t.Hit(20, 9)    ' SELL
  DIM sideSell AS INTEGER
  sideSell = t.Side()
  CONTROL REMOVE t
  IF rBuy = 0 AND sideBuy = 0 AND rSell = 1 AND sideSell = 1 AND t.Qty() = 5 AND ABS(t.Price() - 123.5) < 0.001 THEN
    PRINT "orderticket-ok"
  ELSE
    PRINT "orderticket-no"
  END IF
END SUB
