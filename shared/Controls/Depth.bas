' Depth.bas — Market Depth / DOM (price ladder) come UserControl (§12.4).
' Ladder di prezzi con lati bid/ask, click-to-trade (lift ask / hit bid) e linee
' Stop-Loss / Take-Profit impostabili dal drag. Componente di dominio trading →
' classe BASIC INHERITS Control (parità 4 backend per costruzione).
'
' Nota: niente nuovi verbi/TokenKind — solo la base `Control` + le primitive del
' GraphicsContext; l'interattività (hit-test) vive nel UserControl.
IMPORT CONTROLS

NAMESPACE CustomControls

PUBLIC CLASS MarketDepth INHERITS Control
  PRIVATE DIM mRows AS INTEGER
  PRIVATE DIM mTopPx AS DOUBLE
  PRIVATE DIM mStep AS DOUBLE
  PRIVATE DIM mBestBid AS DOUBLE
  PRIVATE DIM mBestAsk AS DOUBLE
  PRIVATE DIM mSel AS INTEGER
  PRIVATE DIM mSide AS INTEGER          ' -1 n/d, 0 = BUY (lift ask), 1 = SELL (hit bid)
  PRIVATE DIM mLastPx AS DOUBLE
  PRIVATE DIM mStop AS DOUBLE
  PRIVATE DIM mTarget AS DOUBLE

  ' Righe del ladder: `rows` livelli da `topPx` con passo `step`.
  SUB Configure(rows AS INTEGER, topPx AS DOUBLE, step AS DOUBLE)
    mRows = rows : mTopPx = topPx : mStep = step
    mSel = -1 : mSide = -1 : mLastPx = 0.0 : mStop = 0.0 : mTarget = 0.0
  END SUB

  SUB SetQuote(bid AS DOUBLE, ask AS DOUBLE)
    mBestBid = bid : mBestAsk = ask
  END SUB

  FUNCTION Rows() AS INTEGER
    RETURN mRows
  END FUNCTION

  FUNCTION PriceAt(r AS INTEGER) AS DOUBLE
    RETURN mTopPx - r * mStep
  END FUNCTION

  ' Riga del ladder più vicina a un prezzo.
  FUNCTION RowAt(price AS DOUBLE) AS INTEGER
    DIM r AS INTEGER
    IF mStep <= 0.0 THEN RETURN 0
    r = INT((mTopPx - price) / mStep + 0.5)
    IF r < 0 THEN r = 0
    IF r > mRows - 1 THEN r = mRows - 1
    RETURN r
  END FUNCTION

  ' Click-to-trade: click su una riga del ladder (ly = indice riga).
  ' Prezzo >= bestAsk → BUY (lift); prezzo <= bestBid → SELL (hit). Ritorna la riga (-1 se fuori).
  FUNCTION Hit(lx AS INTEGER, ly AS INTEGER) AS INTEGER
    IF ly < 0 OR ly >= mRows THEN RETURN -1
    DIM px AS DOUBLE
    px = PriceAt(ly)
    mSel = ly : mLastPx = px
    IF px >= mBestAsk THEN
      mSide = 0
    ELSE
      IF px <= mBestBid THEN
        mSide = 1
      ELSE
        mSide = -1
      END IF
    END IF
    RETURN ly
  END FUNCTION

  ' Drag dei livelli SL/TP: il drag mappa una posizione al prezzo corrispondente.
  SUB DragStop(price AS DOUBLE)
    mStop = price
  END SUB

  SUB DragTarget(price AS DOUBLE)
    mTarget = price
  END SUB

  FUNCTION SelectedRow() AS INTEGER
    RETURN mSel
  END FUNCTION

  FUNCTION Side() AS INTEGER
    RETURN mSide
  END FUNCTION

  FUNCTION LastPrice() AS DOUBLE
    RETURN mLastPx
  END FUNCTION

  FUNCTION Stop() AS DOUBLE
    RETURN mStop
  END FUNCTION

  FUNCTION Target() AS DOUBLE
    RETURN mTarget
  END FUNCTION

  ' 0 = neutro, 1 = bid (verde), 2 = sopra ask (rosso), 3 = ask (giallo).
  FUNCTION RowKind(px AS DOUBLE) AS INTEGER
    IF px > mBestAsk THEN RETURN 2
    IF px >= mBestAsk THEN RETURN 3
    IF px <= mBestBid THEN RETURN 1
    RETURN 0
  END FUNCTION

  SUB Paint(g AS ANY)
    g.Clear()
    g.SetColor(8421504)
    g.Rect(0, 0, 30, 12)
    DIM r AS INTEGER
    FOR r = 0 TO mRows - 1
      DIM px AS DOUBLE
      px = PriceAt(r)
      DIM k AS INTEGER
      k = RowKind(px)
      IF k = 1 THEN g.SetColor(65280)
      IF k = 2 THEN g.SetColor(16711680)
      IF k = 3 THEN g.SetColor(16776960)
      IF k = 0 THEN g.SetColor(8421504)
      g.Text(STR(px), 1, r)
      IF r = mSel THEN
        g.SetColor(16777215)
        g.Rect(0, r, 30, 1)
      END IF
    NEXT r
    IF mStop > 0.0 THEN
      g.SetColor(16711680)
      g.Line(0, RowAt(mStop), 29, RowAt(mStop))
    END IF
    IF mTarget > 0.0 THEN
      g.SetColor(65280)
      g.Line(0, RowAt(mTarget), 29, RowAt(mTarget))
    END IF
  END SUB
END CLASS
