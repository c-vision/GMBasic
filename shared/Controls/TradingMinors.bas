' TradingMinors.bas — controlli di dominio "minori" (§12.4) come UserControl (BASIC).
' ---------------------------------------------------------------------------
' Veicolo = UserControl (classe BASIC INHERITS Control), NON built-in: nessun verbo/TokenKind
' nuovo; usa solo la base `Control` + il GraphicsContext. Parità 4 backend per costruzione.
' Quattro controlli:
'   • Ticker      — ultimo prezzo con flash verde/rosso sul tick (up/down), poi sbiadisce.
'   • TreeGroup   — posizioni raggruppate (gruppo espandibile/collassabile, totale per gruppo).
'   • EconCalendar— calendario economico: eventi con impatto + filtro per impatto minimo.
'   • Toolbar     — barra di pulsanti/icone (selezione, hit-test).
IMPORT CONTROLS

NAMESPACE CustomControls

' ------------------------------- Ticker ---------------------------------
PUBLIC CLASS Ticker INHERITS Control
  PRIVATE DIM mSym AS STRING
  PRIVATE DIM mLast AS DOUBLE
  PRIVATE DIM mPrev AS DOUBLE
  PRIVATE DIM mDir AS INTEGER
  PRIVATE DIM mFlash AS INTEGER
  PRIVATE DIM mOriginX AS INTEGER
  PRIVATE DIM mOriginY AS INTEGER
  PRIVATE DIM mW AS INTEGER
  PRIVATE DIM mH AS INTEGER

  SUB SetOrigin(ox AS INTEGER, oy AS INTEGER, ww AS INTEGER, hh AS INTEGER)
    mOriginX = ox : mOriginY = oy : mW = ww : mH = hh
  END SUB
  SUB Init(sym AS STRING, price AS DOUBLE)
    mSym = sym : mLast = price : mPrev = price : mDir = 0 : mFlash = 0
  END SUB
  SUB Quote(price AS DOUBLE)
    mPrev = mLast : mLast = price
    IF price > mPrev THEN mDir = 1
    IF price < mPrev THEN mDir = -1
    IF price <> mPrev THEN mFlash = 8
  END SUB
  SUB Tick()
    IF mFlash > 0 THEN mFlash = mFlash - 1
  END SUB
  FUNCTION Direction() AS INTEGER
    RETURN mDir
  END FUNCTION
  FUNCTION Price() AS DOUBLE
    RETURN mLast
  END FUNCTION
  FUNCTION Flash() AS INTEGER
    RETURN mFlash
  END FUNCTION
  FUNCTION Symbol() AS STRING
    RETURN mSym
  END FUNCTION
  SUB Paint(g AS ANY)
    g.Clear()
    IF mFlash > 0 AND mDir > 0 THEN g.SetColor(65280)
    IF mFlash > 0 AND mDir < 0 THEN g.SetColor(16711680)
    IF mFlash = 0 THEN g.SetColor(8421504)
    g.FillRect(0, 0, mW, mH)
    g.SetColor(16777215)
    DIM s AS STRING
    s = mSym + " " + STR(mLast)
    g.Text(s, 1, 0)
  END SUB
END CLASS

' ------------------------------ TreeGroup -------------------------------
PUBLIC CLASS TreeGroup INHERITS Control
  PRIVATE DIM mKind(64) AS INTEGER      ' 1 = gruppo, 0 = posizione
  PRIVATE DIM mParent(64) AS INTEGER    ' riga del gruppo (posizioni), -1 per gruppi
  PRIVATE DIM mName(64) AS STRING
  PRIVATE DIM mQty(64) AS INTEGER
  PRIVATE DIM mOpen(16) AS INTEGER
  PRIVATE DIM mN AS INTEGER
  PRIVATE DIM mOriginX AS INTEGER
  PRIVATE DIM mOriginY AS INTEGER
  PRIVATE DIM mW AS INTEGER
  PRIVATE DIM mH AS INTEGER

  SUB SetOrigin(ox AS INTEGER, oy AS INTEGER, ww AS INTEGER, hh AS INTEGER)
    mOriginX = ox : mOriginY = oy : mW = ww : mH = hh
  END SUB
  SUB AddGroup(name AS STRING)
    IF mN >= 64 THEN RETURN
    mKind(mN) = 1 : mParent(mN) = -1 : mName(mN) = name : mQty(mN) = 0
    mN = mN + 1
  END SUB
  SUB AddPosition(grp AS INTEGER, name AS STRING, qty AS INTEGER)
    IF mN >= 64 THEN RETURN
    mKind(mN) = 0 : mParent(mN) = grp : mName(mN) = name : mQty(mN) = qty
    mN = mN + 1
  END SUB
  FUNCTION Count() AS INTEGER
    RETURN mN
  END FUNCTION
  SUB Toggle(grp AS INTEGER)
    IF mOpen(grp) = 0 THEN mOpen(grp) = 1 ELSE mOpen(grp) = 0
  END SUB
  FUNCTION IsExpanded(grp AS INTEGER) AS INTEGER
    RETURN mOpen(grp)
  END FUNCTION
  FUNCTION VisibleCount() AS INTEGER
    DIM i AS INTEGER
    DIM n AS INTEGER
    n = 0
    FOR i = 0 TO mN - 1
      IF mKind(i) = 1 THEN
        n = n + 1
      ELSE
        IF mParent(i) >= 0 AND mOpen(mParent(i)) <> 0 THEN n = n + 1
      END IF
    NEXT i
    RETURN n
  END FUNCTION
  FUNCTION GroupQty(grp AS INTEGER) AS INTEGER
    DIM i AS INTEGER
    DIM t AS INTEGER
    t = 0
    FOR i = 0 TO mN - 1
      IF mKind(i) = 0 AND mParent(i) = grp THEN t = t + mQty(i)
    NEXT i
    RETURN t
  END FUNCTION
  SUB Paint(g AS ANY)
    g.Clear()
  END SUB
END CLASS

' ---------------------------- EconCalendar ------------------------------
PUBLIC CLASS EconCalendar INHERITS Control
  PRIVATE DIM mTime(32) AS STRING
  PRIVATE DIM mName(32) AS STRING
  PRIVATE DIM mImpact(32) AS INTEGER    ' 0 basso, 1 medio, 2 alto
  PRIVATE DIM mN AS INTEGER
  PRIVATE DIM mMinImpact AS INTEGER
  PRIVATE DIM mOriginX AS INTEGER
  PRIVATE DIM mOriginY AS INTEGER
  PRIVATE DIM mW AS INTEGER
  PRIVATE DIM mH AS INTEGER

  SUB SetOrigin(ox AS INTEGER, oy AS INTEGER, ww AS INTEGER, hh AS INTEGER)
    mOriginX = ox : mOriginY = oy : mW = ww : mH = hh
  END SUB
  SUB AddEvent(t AS STRING, name AS STRING, impact AS INTEGER)
    IF mN >= 32 THEN RETURN
    mTime(mN) = t : mName(mN) = name : mImpact(mN) = impact
    mN = mN + 1
  END SUB
  FUNCTION Count() AS INTEGER
    RETURN mN
  END FUNCTION
  SUB SetMinImpact(k AS INTEGER)
    mMinImpact = k
  END SUB
  FUNCTION MatchCount() AS INTEGER
    DIM i AS INTEGER
    DIM n AS INTEGER
    n = 0
    FOR i = 0 TO mN - 1
      IF mImpact(i) >= mMinImpact THEN n = n + 1
    NEXT i
    RETURN n
  END FUNCTION
  FUNCTION EventName(i AS INTEGER) AS STRING
    IF i < 0 OR i > mN - 1 THEN RETURN ""
    RETURN mName(i)
  END FUNCTION
  SUB Paint(g AS ANY)
    g.Clear()
  END SUB
END CLASS

' ------------------------------ Toolbar ---------------------------------
PUBLIC CLASS Toolbar INHERITS Control
  PRIVATE DIM mLbl(16) AS STRING
  PRIVATE DIM mN AS INTEGER
  PRIVATE DIM mSel AS INTEGER
  PRIVATE DIM mVertical AS INTEGER
  PRIVATE DIM mOriginX AS INTEGER
  PRIVATE DIM mOriginY AS INTEGER
  PRIVATE DIM mW AS INTEGER
  PRIVATE DIM mH AS INTEGER

  SUB SetOrigin(ox AS INTEGER, oy AS INTEGER, ww AS INTEGER, hh AS INTEGER)
    mOriginX = ox : mOriginY = oy : mW = ww : mH = hh
  END SUB
  SUB Configure(vertical AS INTEGER)
    mVertical = vertical : mSel = 0
  END SUB
  SUB AddButton(label AS STRING)
    IF mN < 16 THEN
      mLbl(mN) = label
      mN = mN + 1
    END IF
  END SUB
  FUNCTION Count() AS INTEGER
    RETURN mN
  END FUNCTION
  SUB SetSelected(i AS INTEGER)
    IF i >= 0 AND i <= mN - 1 THEN mSel = i
  END SUB
  FUNCTION Selected() AS INTEGER
    RETURN mSel
  END FUNCTION
  FUNCTION SelectedText() AS STRING
    IF mSel < 0 OR mSel > mN - 1 THEN RETURN ""
    RETURN mLbl(mSel)
  END FUNCTION
  FUNCTION ButtonAt(lx AS INTEGER, ly AS INTEGER) AS INTEGER
    IF mN <= 0 THEN RETURN -1
    IF mW < 1 OR mH < 1 THEN RETURN -1
    IF lx < 0 OR ly < 0 OR lx >= mW OR ly >= mH THEN RETURN -1
    DIM idx AS INTEGER
    IF mVertical <> 0 THEN
      DIM r AS DOUBLE
      r = ly : r = r * mN : r = r / mH
      idx = INT(r)
    ELSE
      DIM c AS DOUBLE
      c = lx : c = c * mN : c = c / mW
      idx = INT(c)
    END IF
    IF idx < 0 THEN idx = 0
    IF idx > mN - 1 THEN idx = mN - 1
    RETURN idx
  END FUNCTION
  FUNCTION Hit(lx AS INTEGER, ly AS INTEGER) AS INTEGER
    DIM i AS INTEGER
    i = ButtonAt(lx, ly)
    IF i >= 0 THEN mSel = i
    RETURN i
  END FUNCTION
  SUB Paint(g AS ANY)
    g.Clear()
  END SUB
END CLASS
