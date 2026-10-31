' CandleChart.bas — chart a candele INTERATTIVO come UserControl (stile VB6), in BASIC.
' ---------------------------------------------------------------------------
' Veicolo = UserControl (classe BASIC INHERITS Control), NON built-in: nessun
' verbo/TokenKind nuovo, usa solo la base `Control` + le primitive del
' GraphicsContext. Parità 4 backend per costruzione.
'
' Funzioni:
'   • candele OHLC (AddCandle)
'   • finestra di vista x0/count (zoom/pan, Fit)
'   • autoscale verticale (o range esplicito con SetYRange)
'   • crosshair + Data Window (OHLC sotto il cursore)
'   • drawing tools in coordinate DATI: trend/hline/vline/rect/fib
'     con selezione (PickAt), spostamento (MoveSelected) e cancellazione
'   • overlay indicatori: SMA / EMA
'   • marker trade/signal (buy/sell/neutro)
' L'interazione via mouse e' delegata all'host: i metodi *explicit* con
' coordinate LOCALI (lx,ly) rendono il controllo testabile headless.
IMPORT CONTROLS

NAMESPACE CustomControls

PUBLIC CLASS CandleChart INHERITS Control
  ' ---------------- dati OHLC ----------------
  PRIVATE DIM mOpen(128) AS DOUBLE
  PRIVATE DIM mHigh(128) AS DOUBLE
  PRIVATE DIM mLow(128) AS DOUBLE
  PRIVATE DIM mClose(128) AS DOUBLE
  PRIVATE DIM mN AS INTEGER

  ' ---------------- vista (finestra x + range y) ----------------
  PRIVATE DIM mX0 AS INTEGER          ' primo indice visibile
  PRIVATE DIM mXVIS AS INTEGER        ' numero candele visibili
  PRIVATE DIM mYMin AS DOUBLE
  PRIVATE DIM mYMax AS DOUBLE
  PRIVATE DIM mAutoY AS INTEGER       ' 0 = range esplicito, 1 = autoscale

  ' ---------------- geometria ----------------
  PRIVATE DIM mOriginX AS INTEGER
  PRIVATE DIM mOriginY AS INTEGER
  PRIVATE DIM mW AS INTEGER
  PRIVATE DIM mH AS INTEGER

  ' ---------------- crosshair ----------------
  PRIVATE DIM mCross AS INTEGER       ' indice candela sotto il cursore (-1 = nessuno)
  PRIVATE DIM mCrossPrice AS DOUBLE   ' prezzo Y sotto il cursore

  ' ---------------- drawing tools ----------------
  PRIVATE DIM mDrawType(16) AS INTEGER   ' 1=trend 2=hline 3=vline 4=rect 5=fib
  PRIVATE DIM mDX1(16) AS DOUBLE
  PRIVATE DIM mDY1(16) AS DOUBLE
  PRIVATE DIM mDX2(16) AS DOUBLE
  PRIVATE DIM mDY2(16) AS DOUBLE
  PRIVATE DIM mDrawN AS INTEGER
  PRIVATE DIM mDrawSel AS INTEGER

  ' ---------------- overlay ----------------
  PRIVATE DIM mSma(128) AS DOUBLE
  PRIVATE DIM mEma(128) AS DOUBLE
  PRIVATE DIM mOverlay AS INTEGER     ' 0=none 1=SMA 2=EMA 3=entrambe
  PRIVATE DIM mPeriod AS INTEGER

  ' ---------------- marker ----------------
  PRIVATE DIM mMarkI(32) AS INTEGER
  PRIVATE DIM mMarkP(32) AS DOUBLE
  PRIVATE DIM mMarkK(32) AS INTEGER   ' 0=buy 1=sell 2=neutro
  PRIVATE DIM mMarkN AS INTEGER

  ' ================= dati =================
  SUB AddCandle(po AS DOUBLE, ph AS DOUBLE, pl AS DOUBLE, pc AS DOUBLE)
    IF mN < 128 THEN
      mOpen(mN) = po
      mHigh(mN) = ph
      mLow(mN) = pl
      mClose(mN) = pc
      mN = mN + 1
    END IF
  END SUB

  FUNCTION Count() AS INTEGER
    RETURN mN
  END FUNCTION

  SUB SetOrigin(ox AS INTEGER, oy AS INTEGER, ww AS INTEGER, hh AS INTEGER)
    mOriginX = ox : mOriginY = oy : mW = ww : mH = hh
    IF mAutoY = 0 AND mYMax <= mYMin THEN mAutoY = 1
  END SUB

  ' ================= vista =================
  SUB SetView(first AS INTEGER, count AS INTEGER)
    DIM f AS INTEGER
    f = first
    IF f < 0 THEN f = 0
    IF mN > 0 THEN
      IF f > mN - 1 THEN f = mN - 1
    ELSE
      f = 0
    END IF
    DIM c AS INTEGER
    c = count
    IF c < 1 THEN c = 1
    DIM avail AS INTEGER
    avail = mN - f
    IF avail < 1 THEN avail = 1
    IF c > avail THEN c = avail
    mX0 = f : mXVIS = c
  END SUB

  SUB Fit()
    mX0 = 0
    mXVIS = mN
    IF mXVIS < 1 THEN mXVIS = 1
  END SUB

  FUNCTION ViewFirst() AS INTEGER
    RETURN mX0
  END FUNCTION

  FUNCTION ViewCount() AS INTEGER
    RETURN mXVIS
  END FUNCTION

  ' Pan di `d` candele (positivo = verso destra nel tempo).
  SUB PanBy(d AS INTEGER)
    SetView(mX0 + d, mXVIS)
  END SUB

  ' Zoom attorno alla colonna `lx` (factor > 1 = allarga la finestra).
  SUB ZoomAt(factor AS DOUBLE, lx AS INTEGER)
    IF mN <= 0 THEN RETURN
    IF factor <= 0.0 THEN RETURN
    IF mW <= 0 THEN RETURN
    ' indice (frazionario) sotto il cursore
    DIM r AS DOUBLE
    r = lx : r = r * mXVIS : r = r / mW : r = mX0 + r
    DIM nv AS INTEGER
    nv = INT(mXVIS * factor + 0.5)
    IF nv < 4 THEN nv = 4
    IF nv > mN THEN nv = mN
    ' mantieni r alla stessa colonna: nx0 = r - lx*nv/mW
    DIM nf AS DOUBLE
    nf = lx : nf = nf * nv : nf = nf / mW
    DIM nx0 AS DOUBLE
    nx0 = r - nf
    SetView(INT(nx0 + 0.5), nv)
  END SUB

  ' Range verticale esplicito (disattiva l'autoscale).
  SUB SetYRange(lo AS DOUBLE, hi AS DOUBLE)
    IF hi <= lo THEN hi = lo + 1.0
    mYMin = lo : mYMax = hi : mAutoY = 0
  END SUB

  SUB AutoScaleY()
    mAutoY = 1
  END SUB

  ' ================= crosshair =================
  SUB SetCrosshair(lx AS INTEGER, ly AS INTEGER)
    IF mN <= 0 THEN RETURN
    UpdateY()
    mCross = IdxAt(lx)
    mCrossPrice = PriceForY(ly)
  END SUB

  SUB ClearCrosshair()
    mCross = -1
    mCrossPrice = 0.0
  END SUB

  FUNCTION CrossIndex() AS INTEGER
    RETURN mCross
  END FUNCTION

  FUNCTION CrossPrice() AS DOUBLE
    RETURN mCrossPrice
  END FUNCTION

  FUNCTION CrossClose() AS DOUBLE
    IF mCross < 0 OR mCross > mN - 1 THEN RETURN 0.0
    RETURN mClose(mCross)
  END FUNCTION

  FUNCTION CrossOpen() AS DOUBLE
    IF mCross < 0 OR mCross > mN - 1 THEN RETURN 0.0
    RETURN mOpen(mCross)
  END FUNCTION

  ' ================= drawing tools (coordinate DATI) =================
  FUNCTION AddTrend(ix1 AS DOUBLE, iy1 AS DOUBLE, ix2 AS DOUBLE, iy2 AS DOUBLE) AS INTEGER
    RETURN AddDrawing(1, ix1, iy1, ix2, iy2)
  END FUNCTION
  FUNCTION AddHLine(price AS DOUBLE) AS INTEGER
    RETURN AddDrawing(2, 0.0, price, 0.0, price)
  END FUNCTION
  FUNCTION AddVLine(ix AS DOUBLE) AS INTEGER
    RETURN AddDrawing(3, ix, 0.0, ix, 0.0)
  END FUNCTION
  FUNCTION AddRect(ix1 AS DOUBLE, iy1 AS DOUBLE, ix2 AS DOUBLE, iy2 AS DOUBLE) AS INTEGER
    RETURN AddDrawing(4, ix1, iy1, ix2, iy2)
  END FUNCTION
  FUNCTION AddFib(ix1 AS DOUBLE, iy1 AS DOUBLE, ix2 AS DOUBLE, iy2 AS DOUBLE) AS INTEGER
    RETURN AddDrawing(5, ix1, iy1, ix2, iy2)
  END FUNCTION

  FUNCTION AddDrawing(t AS INTEGER, ix1 AS DOUBLE, iy1 AS DOUBLE, ix2 AS DOUBLE, iy2 AS DOUBLE) AS INTEGER
    IF mDrawN >= 16 THEN RETURN -1
    mDrawType(mDrawN) = t
    mDX1(mDrawN) = ix1 : mDY1(mDrawN) = iy1
    mDX2(mDrawN) = ix2 : mDY2(mDrawN) = iy2
    mDrawN = mDrawN + 1
    RETURN mDrawN - 1
  END FUNCTION

  FUNCTION DrawingCount() AS INTEGER
    RETURN mDrawN
  END FUNCTION

  FUNCTION SelectedDrawing() AS INTEGER
    RETURN mDrawSel
  END FUNCTION

  FUNCTION DrawingY2(i AS INTEGER) AS DOUBLE
    IF i < 0 OR i >= mDrawN THEN RETURN 0.0
    RETURN mDY2(i)
  END FUNCTION

  SUB ClearDrawings()
    mDrawN = 0 : mDrawSel = -1
  END SUB

  ' Seleziona il drawing piu' vicino a (lx,ly); -1 se nessuno entro la soglia.
  FUNCTION PickAt(lx AS INTEGER, ly AS INTEGER) AS INTEGER
    IF mN <= 0 THEN RETURN -1
    UpdateY()
    DIM i AS INTEGER
    DIM best AS DOUBLE
    best = 999999.0
    DIM bestI AS INTEGER
    bestI = -1
    FOR i = 0 TO mDrawN - 1
      DIM d AS DOUBLE
      d = DistSq(lx, ly, i)
      IF d < best THEN best = d : bestI = i
    NEXT i
    IF best > 36.0 THEN bestI = -1
    mDrawSel = bestI
    RETURN mDrawSel
  END FUNCTION

  ' Sposta l'estremo (x2,y2) del drawing selezionato sotto il cursore (hline: y; vline: x).
  SUB MoveSelected(lx AS INTEGER, ly AS INTEGER)
    IF mDrawSel < 0 OR mDrawSel >= mDrawN THEN RETURN
    UpdateY()
    DIM t AS INTEGER
    t = mDrawType(mDrawSel)
    IF t = 2 THEN
      mDY1(mDrawSel) = PriceForY(ly)
      mDY2(mDrawSel) = PriceForY(ly)
      RETURN
    END IF
    IF t = 3 THEN
      mDX1(mDrawSel) = IdxAt(lx)
      mDX2(mDrawSel) = IdxAt(lx)
      RETURN
    END IF
    mDX2(mDrawSel) = IdxAt(lx)
    mDY2(mDrawSel) = PriceForY(ly)
  END SUB

  ' Cancella il drawing selezionato (compatta l'array).
  SUB DeleteSelected()
    IF mDrawSel < 0 OR mDrawSel >= mDrawN THEN RETURN
    UpdateY()
    DIM i AS INTEGER
    FOR i = mDrawSel TO mDrawN - 2
      mDrawType(i) = mDrawType(i + 1)
      mDX1(i) = mDX1(i + 1) : mDY1(i) = mDY1(i + 1)
      mDX2(i) = mDX2(i + 1) : mDY2(i) = mDY2(i + 1)
    NEXT i
    mDrawN = mDrawN - 1
    mDrawSel = -1
  END SUB

  ' ================= overlay (SMA / EMA) =================
  SUB SetOverlay(kind AS INTEGER, period AS INTEGER)
    mOverlay = kind
    IF period < 2 THEN period = 2
    mPeriod = period
    ' SMA (finestra scorrevole, un solo ciclo)
    DIM run AS DOUBLE
    run = 0.0
    DIM i AS INTEGER
    FOR i = 0 TO mN - 1
      run = run + mClose(i)
      IF i >= period THEN run = run - mClose(i - period)
      IF i >= period - 1 THEN
        mSma(i) = run / period
      ELSE
        mSma(i) = 0.0
      END IF
    NEXT i
    ' EMA
    DIM k AS DOUBLE
    k = 2.0 / (period + 1)
    DIM prev AS DOUBLE
    prev = 0.0
    FOR i = 0 TO mN - 1
      IF i = 0 THEN
        prev = mClose(0)
      ELSE
        prev = mClose(i) * k + prev * (1.0 - k)
      END IF
      mEma(i) = prev
    NEXT i
  END SUB

  FUNCTION OverlayReady() AS INTEGER
    IF mOverlay = 0 THEN RETURN 0
    IF mN < mPeriod THEN RETURN 0
    RETURN 1
  END FUNCTION

  FUNCTION SmaValue(i AS INTEGER) AS DOUBLE
    IF i < 0 OR i > mN - 1 THEN RETURN 0.0
    RETURN mSma(i)
  END FUNCTION

  FUNCTION EmaValue(i AS INTEGER) AS DOUBLE
    IF i < 0 OR i > mN - 1 THEN RETURN 0.0
    RETURN mEma(i)
  END FUNCTION

  ' ================= marker =================
  SUB AddMarker(ix AS INTEGER, price AS DOUBLE, kind AS INTEGER)
    IF mMarkN >= 32 THEN RETURN
    mMarkI(mMarkN) = ix : mMarkP(mMarkN) = price : mMarkK(mMarkN) = kind
    mMarkN = mMarkN + 1
  END SUB

  FUNCTION MarkerCount() AS INTEGER
    RETURN mMarkN
  END FUNCTION

  ' ================= mapping (geometria) =================
  SUB UpdateY()
    IF mN <= 0 THEN RETURN
    IF mAutoY = 0 AND mYMax > mYMin THEN RETURN
    DIM last AS INTEGER
    last = mX0 + mXVIS - 1
    IF last > mN - 1 THEN last = mN - 1
    IF mX0 < 0 THEN mX0 = 0
    DIM mn AS DOUBLE
    DIM mx AS DOUBLE
    mn = mLow(mX0)
    mx = mHigh(mX0)
    DIM i AS INTEGER
    FOR i = mX0 TO last
      IF mLow(i) < mn THEN mn = mLow(i)
      IF mHigh(i) > mx THEN mx = mHigh(i)
    NEXT i
    IF mx <= mn THEN mx = mn + 1.0
    mYMin = mn : mYMax = mx
  END SUB

  FUNCTION XOf(ix AS INTEGER) AS DOUBLE
    IF mXVIS <= 0 THEN RETURN 0.0
    DIM stepw AS DOUBLE
    stepw = mW
    stepw = stepw / mXVIS
    DIM x0 AS DOUBLE
    x0 = ix - mX0
    RETURN x0 * stepw + stepw / 2.0
  END FUNCTION

  FUNCTION YOf(p AS DOUBLE) AS DOUBLE
    DIM rng AS DOUBLE
    rng = mYMax - mYMin
    IF rng <= 0.0 THEN RETURN 0.0
    DIM f AS DOUBLE
    f = (mYMax - p) / rng
    DIM hh AS DOUBLE
    hh = mH - 1
    RETURN f * hh
  END FUNCTION

  FUNCTION IdxAt(lx AS INTEGER) AS INTEGER
    IF mW <= 0 THEN RETURN mX0
    DIM r AS DOUBLE
    r = lx : r = r * mXVIS : r = r / mW : r = mX0 + r
    DIM ix AS INTEGER
    ix = INT(r + 0.5)
    IF ix < mX0 THEN ix = mX0
    DIM lastI AS INTEGER
    lastI = mX0 + mXVIS - 1
    IF ix > lastI THEN ix = lastI
    IF ix > mN - 1 THEN ix = mN - 1
    IF ix < 0 THEN ix = 0
    RETURN ix
  END FUNCTION

  FUNCTION PriceForY(ly AS INTEGER) AS DOUBLE
    DIM rng AS DOUBLE
    rng = mYMax - mYMin
    DIM hh AS DOUBLE
    hh = mH - 1
    IF hh <= 0.0 THEN RETURN mYMax
    DIM f AS DOUBLE
    f = ly : f = f / hh
    RETURN mYMax - f * rng
  END FUNCTION

  ' Distanza^2 (pixel) del drawing i dal punto (lx,ly).
  FUNCTION DistSq(lx AS INTEGER, ly AS INTEGER, i AS INTEGER) AS DOUBLE
    DIM t AS INTEGER
    t = mDrawType(i)
    DIM dx AS DOUBLE
    DIM dy AS DOUBLE
    IF t = 2 THEN
      dy = ly - YOf(mDY1(i))
      RETURN dy * dy
    END IF
    IF t = 3 THEN
      dx = lx - XOf(INT(mDX1(i)))
      RETURN dx * dx
    END IF
    DIM mx AS DOUBLE
    DIM my AS DOUBLE
    mx = (XOf(INT(mDX1(i))) + XOf(INT(mDX2(i)))) / 2.0
    my = (YOf(mDY1(i)) + YOf(mDY2(i))) / 2.0
    dx = lx - mx
    dy = ly - my
    RETURN dx * dx + dy * dy
  END FUNCTION

  ' ================= rendering =================
  SUB Paint(g AS ANY)
    g.Clear()
    IF mW < 2 OR mH < 2 THEN RETURN
    g.SetColor(8421504)
    g.Rect(0, 0, mW - 1, mH - 1)
    IF mN <= 0 THEN RETURN
    UpdateY()

    DIM last AS INTEGER
    last = mX0 + mXVIS - 1
    IF last > mN - 1 THEN last = mN - 1

    ' ---- candele ----
    DIM cw AS DOUBLE
    cw = mW
    cw = cw / mXVIS
    IF cw < 1.0 THEN cw = 1.0
    DIM halfcw AS DOUBLE
    halfcw = cw / 2.0
    DIM i AS INTEGER
    FOR i = mX0 TO last
      IF mClose(i) >= mOpen(i) THEN
        g.SetColor(65280)
      ELSE
        g.SetColor(16711680)
      END IF
      DIM xc AS DOUBLE
      xc = XOf(i)
      DIM xi AS INTEGER
      xi = INT(xc + 0.5)
      g.Line(xi, INT(YOf(mHigh(i))), xi, INT(YOf(mLow(i))))
      DIM yt AS DOUBLE
      DIM yb AS DOUBLE
      IF mOpen(i) > mClose(i) THEN
        yt = YOf(mOpen(i)) : yb = YOf(mClose(i))
      ELSE
        yt = YOf(mClose(i)) : yb = YOf(mOpen(i))
      END IF
      DIM bh AS DOUBLE
      bh = yb - yt
      IF bh < 1.0 THEN bh = 1.0
      DIM bx AS DOUBLE
      bx = xc - halfcw
      g.FillRect(INT(bx + 0.5), INT(yt + 0.5), INT(cw), INT(bh))
    NEXT i

    ' ---- overlay SMA (giallo) / EMA (ciano) ----
    IF mOverlay = 1 OR mOverlay = 3 THEN
      g.SetColor(16776960)
      DIM started AS INTEGER
      DIM px AS DOUBLE
      DIM py AS DOUBLE
      started = 0
      FOR i = mX0 TO last
        IF i >= mPeriod - 1 THEN
          DIM cx AS DOUBLE
          DIM cy AS DOUBLE
          cx = XOf(i)
          cy = YOf(mSma(i))
          IF started = 1 THEN g.Line(INT(px), INT(py), INT(cx), INT(cy))
          px = cx : py = cy : started = 1
        END IF
      NEXT i
    END IF
    IF mOverlay = 2 OR mOverlay = 3 THEN
      g.SetColor(65535)
      DIM started2 AS INTEGER
      DIM px2 AS DOUBLE
      DIM py2 AS DOUBLE
      started2 = 0
      FOR i = mX0 TO last
        DIM cx2 AS DOUBLE
        DIM cy2 AS DOUBLE
        cx2 = XOf(i)
        cy2 = YOf(mEma(i))
        IF started2 = 1 THEN g.Line(INT(px2), INT(py2), INT(cx2), INT(cy2))
        px2 = cx2 : py2 = cy2 : started2 = 1
      NEXT i
    END IF

    ' ---- marker ----
    FOR i = 0 TO mMarkN - 1
      DIM mi AS INTEGER
      mi = mMarkI(i)
      IF mi >= mX0 AND mi <= last THEN
        DIM mx AS DOUBLE
        DIM my AS DOUBLE
        mx = XOf(mi)
        my = YOf(mMarkP(i))
        IF mMarkK(i) = 0 THEN g.SetColor(65280)
        IF mMarkK(i) = 1 THEN g.SetColor(16711680)
        IF mMarkK(i) = 2 THEN g.SetColor(16776960)
        g.FillRect(INT(mx) - 1, INT(my) - 1, 2, 2)
      END IF
    NEXT i

    ' ---- drawing tools ----
    FOR i = 0 TO mDrawN - 1
      IF i = mDrawSel THEN
        g.SetColor(16777215)
      ELSE
        g.SetColor(16727871)
      END IF
      DIM t AS INTEGER
      t = mDrawType(i)
      IF t = 1 THEN
        g.Line(INT(XOf(INT(mDX1(i)))), INT(YOf(mDY1(i))), INT(XOf(INT(mDX2(i)))), INT(YOf(mDY2(i))))
      END IF
      IF t = 2 THEN
        DIM hy AS DOUBLE
        hy = YOf(mDY1(i))
        g.Line(0, INT(hy), mW - 1, INT(hy))
      END IF
      IF t = 3 THEN
        DIM vx AS DOUBLE
        vx = XOf(INT(mDX1(i)))
        g.Line(INT(vx), 0, INT(vx), mH - 1)
      END IF
      IF t = 4 THEN
        DIM x1 AS DOUBLE
        DIM x2 AS DOUBLE
        DIM y1 AS DOUBLE
        DIM y2 AS DOUBLE
        x1 = XOf(INT(mDX1(i))) : x2 = XOf(INT(mDX2(i)))
        y1 = YOf(mDY1(i)) : y2 = YOf(mDY2(i))
        IF x2 < x1 THEN
          DIM tx AS DOUBLE
          tx = x1 : x1 = x2 : x2 = tx
        END IF
        IF y2 < y1 THEN
          DIM ty AS DOUBLE
          ty = y1 : y1 = y2 : y2 = ty
        END IF
        g.Rect(INT(x1), INT(y1), INT(x2 - x1), INT(y2 - y1))
      END IF
      IF t = 5 THEN
        ' ritracciamento di Fibonacci: 5 livelli tra y1 e y2
        DIM fy1 AS DOUBLE
        DIM fy2 AS DOUBLE
        fy1 = mDY1(i) : fy2 = mDY2(i)
        DIM lv AS INTEGER
        FOR lv = 0 TO 4
          DIM frac AS DOUBLE
          frac = lv / 4.0
          DIM yy AS DOUBLE
          yy = fy1 + (fy2 - fy1) * frac
          g.Line(INT(XOf(INT(mDX1(i)))), INT(YOf(yy)), INT(XOf(INT(mDX2(i)))), INT(YOf(yy)))
        NEXT lv
      END IF
    NEXT i

    ' ---- crosshair + Data Window ----
    IF mCross >= 0 AND mCross <= mN - 1 THEN
      g.SetColor(16777215)
      DIM crx AS DOUBLE
      crx = XOf(mCross)
      g.Line(INT(crx), 0, INT(crx), mH - 1)
      DIM cry AS DOUBLE
      cry = YOf(mCrossPrice)
      g.Line(0, INT(cry), mW - 1, INT(cry))
      g.SetColor(16776960)
      g.Text("t" + STR(mCross) + " O" + STR(mOpen(mCross)) + " H" + STR(mHigh(mCross)) + " L" + STR(mLow(mCross)) + " C" + STR(mClose(mCross)) + " @" + STR(mCrossPrice), 1, 1)
    END IF
  END SUB
END CLASS
