' OrderTicketDemo.bas — demo d'applicazione del controllo (Controls/OrderTicket.bas via IMPORT).
IMPORT CustomControls

SUB Main()
  DIM t AS OrderTicket = NEW OrderTicket()
  CONTROL ADD t AT 2, 2 SIZE 38, 12
  t.SetOrigin(2, 2)
  t.SetQty(5)
  t.SetPrice(123.5)
  DIM r AS INTEGER
  r = t.Hit(6, 9)     ' click su BUY
  PRINT "side="; t.Side(); " qty="; t.Qty(); " price="; t.Price()
  r = t.Hit(20, 9)    ' click su SELL
  PRINT "side2="; t.Side()
  CONTROL REMOVE t
  PRINT "orderticket-ok"
END SUB
