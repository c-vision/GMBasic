' OrderTicket.bas — Order Ticket (stile MT5) come UserControl (§12.4).
' Composite DISEGNATO in Paint (etichette + campi + bottoni BUY/SELL); interazione via MouseDown
' (hit-test locale, coords da MOUSE.X/Y). Stato esposto con metodi/proprietà.
'
' Nota: componente UI di dominio → classi BASIC INHERITS Control (parità 4 backend per costruzione).
IMPORT CONTROLS

NAMESPACE CustomControls

PUBLIC CLASS OrderTicket INHERITS Control
  PRIVATE DIM mSide AS INTEGER      ' 0 = BUY, 1 = SELL
  PRIVATE DIM mQty AS INTEGER
  PRIVATE DIM mPrice AS DOUBLE
  PRIVATE DIM mOriginX AS INTEGER
  PRIVATE DIM mOriginY AS INTEGER

  ' Origine del controllo sullo schermo (per convertire MOUSE.X/Y in coordinate locali 0..W/0..H).
  SUB SetOrigin(ox AS INTEGER, oy AS INTEGER)
    mOriginX = ox : mOriginY = oy
    mSide = 0 : mQty = 1 : mPrice = 100.0
  END SUB

  SUB SetQty(q AS INTEGER)
    IF q > 0 THEN mQty = q
  END SUB
  SUB SetPrice(p AS DOUBLE)
    IF p > 0 THEN mPrice = p
  END SUB
  FUNCTION Side() AS INTEGER
    RETURN mSide
  END FUNCTION
  FUNCTION Qty() AS INTEGER
    RETURN mQty
  END FUNCTION
  FUNCTION Price() AS DOUBLE
    RETURN mPrice
  END FUNCTION

  ' Hit-test nel rettangolo locale (0..38 x 0..12): ritorna 0=BUY, 1=SELL, -1=fuori.
  FUNCTION Hit(lx AS INTEGER, ly AS INTEGER) AS INTEGER
    IF ly >= 8 AND ly <= 10 THEN
      IF lx >= 2 AND lx <= 13 THEN mSide = 0 : RETURN 0
      IF lx >= 15 AND lx <= 26 THEN mSide = 1 : RETURN 1
    END IF
    RETURN -1
  END FUNCTION

  SUB MouseDown()
    DIM lx AS INTEGER, ly AS INTEGER
    lx = MOUSE.X() - mOriginX
    ly = MOUSE.Y() - mOriginY
    DIM _r AS INTEGER
    _r = SELF.Hit(lx, ly)
  END SUB

  SUB Paint(g AS ANY)
    g.Clear()
    ' bordo
    g.SetColor(8421504)
    g.Rect(0, 0, 38, 12)
    ' titolo
    g.SetColor(16777215)
    g.Text("ORDER TICKET", 2, 1)
    ' campi
    g.SetColor(12632256)
    g.Text("Qty: " + STR(mQty), 2, 3)
    g.Text("Price: " + STR(mPrice), 2, 5)
    ' bottoni: BUY (verde) / SELL (rosso)
    g.SetColor(65280)
    g.FillRect(2, 8, 11, 3)
    g.SetColor(16711680)
    g.FillRect(15, 8, 11, 3)
    ' side corrente evidenziato
    g.SetColor(16777215)
    IF mSide = 0 THEN g.Text("BUY", 4, 9) ELSE g.Text("SELL", 17, 9)
  END SUB
END CLASS
