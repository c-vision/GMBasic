' ContextMenu.bas — Menu contestuale (popup) come UserControl (§12.4).
' Voci attivabili/disabilitate, separatori, navigazione da tastiera/mouse, voce scelta.
' Popup CHIUSO di default (si apre con Open). Componente di dominio/UI → classe BASIC
' INHERITS Control (parità 4 backend).
'
' NB: niente MOUSE.X() (non disponibile su C99Native): l'host inoltra le coordinate LOCALI.
IMPORT CONTROLS

NAMESPACE CustomControls

PUBLIC CLASS ContextMenu INHERITS Control
  PRIVATE DIM mN AS INTEGER
  PRIVATE DIM mItems(16) AS STRING
  PRIVATE DIM mSep(16) AS INTEGER        ' 1 = separatore
  PRIVATE DIM mEn(16) AS INTEGER         ' 1 = abilitata
  PRIVATE DIM mSel AS INTEGER
  PRIVATE DIM mOpen AS INTEGER           ' 0 = chiuso (default)
  PRIVATE DIM mChosen AS INTEGER         ' indice scelto (-1 nessuno)
  PRIVATE DIM mW AS INTEGER
  PRIVATE DIM mLineH AS INTEGER
  PRIVATE DIM mX AS INTEGER              ' posizione popup (coordinate locali del controllo)
  PRIVATE DIM mY AS INTEGER
  PRIVATE DIM mCBg AS DOUBLE
  PRIVATE DIM mCFg AS DOUBLE
  PRIVATE DIM mCSelBg AS DOUBLE
  PRIVATE DIM mCSelFg AS DOUBLE
  PRIVATE DIM mCDis AS DOUBLE
  PRIVATE DIM mCBorder AS DOUBLE

  SUB Configure(wid AS INTEGER)
    mN = 0 : mSel = -1 : mOpen = 0 : mChosen = -1
    mW = wid : mLineH = 1 : mX = 0 : mY = 0
    mCBg = 1973790
    mCFg = 16777215
    mCSelBg = 33023
    mCSelFg = 0
    mCDis = 8421504
    mCBorder = 16776960
  END SUB

  SUB SetTheme(bg AS DOUBLE, fg AS DOUBLE, selBg AS DOUBLE, selFg AS DOUBLE, dis AS DOUBLE, borderCol AS DOUBLE)
    mCBg = bg : mCFg = fg : mCSelBg = selBg : mCSelFg = selFg : mCDis = dis : mCBorder = borderCol
  END SUB

  SUB SetItem(txt AS STRING)
    IF mN >= 16 THEN RETURN
    mItems(mN) = txt : mSep(mN) = 0 : mEn(mN) = 1
    mN = mN + 1
  END SUB

  SUB SetSeparator(txt AS STRING)
    IF mN >= 16 THEN RETURN
    mItems(mN) = txt : mSep(mN) = 1 : mEn(mN) = 0
    mN = mN + 1
  END SUB

  SUB SetEnabled(i AS INTEGER, e AS INTEGER)
    IF i >= 0 AND i < mN THEN mEn(i) = e
  END SUB

  ' =============== letture ===============
  FUNCTION Count() AS INTEGER
    RETURN mN
  END FUNCTION
  FUNCTION ItemText(i AS INTEGER) AS STRING
    IF i < 0 OR i >= mN THEN RETURN ""
    RETURN mItems(i)
  END FUNCTION
  FUNCTION IsOpen() AS INTEGER
    RETURN mOpen
  END FUNCTION
  FUNCTION Sel() AS INTEGER
    RETURN mSel
  END FUNCTION
  FUNCTION SelText() AS STRING
    RETURN ItemText(mSel)
  END FUNCTION
  FUNCTION Chosen() AS INTEGER
    RETURN mChosen
  END FUNCTION
  FUNCTION ChosenText() AS STRING
    RETURN ItemText(mChosen)
  END FUNCTION
  FUNCTION MenuWidth() AS INTEGER
    RETURN mW
  END FUNCTION

  ' =============== apertura/chiusura ===============
  SUB Popup(x AS INTEGER, y AS INTEGER)
    mX = x : mY = y : mOpen = 1 : mChosen = -1
    ' prima voce abilitata non-separatore
    mSel = -1
    DIM i AS INTEGER
    FOR i = 0 TO mN - 1
      IF mSel < 0 AND mSep(i) = 0 AND mEn(i) = 1 THEN mSel = i
    NEXT i
  END SUB

  SUB Dismiss()
    mOpen = 0
  END SUB

  ' =============== navigazione ===============
  SUB MoveSel(d AS INTEGER)
    DIM i AS INTEGER
    DIM n AS INTEGER
    IF mN = 0 THEN RETURN
    n = mSel
    FOR i = 1 TO mN
      n = n + d
      IF n < 0 THEN n = mN - 1
      IF n > mN - 1 THEN n = 0
      IF mSep(n) = 0 AND mEn(n) = 1 THEN
        mSel = n
        i = mN + 1
      END IF
    NEXT i
  END SUB

  ' =============== attivazione ===============
  SUB Activate()
    IF mSel >= 0 AND mSel < mN AND mSep(mSel) = 0 AND mEn(mSel) = 1 THEN
      mChosen = mSel
    END IF
    Dismiss()
  END SUB

  ' =============== hit-test (coordinate locali del controllo) ===============
  FUNCTION HitTest(lx AS INTEGER, ly AS INTEGER) AS INTEGER
    IF mOpen = 0 THEN RETURN -1
    IF lx < 0 OR lx > mW THEN RETURN -1
    IF ly < 0 THEN RETURN -1
    DIM r AS INTEGER
    r = ly - mY
    IF r < 0 OR r >= mN THEN RETURN -1
    IF mSep(r) <> 0 THEN RETURN -1
    RETURN r
  END FUNCTION

  ' =============== mouse ===============
  SUB MousePress(lx AS INTEGER, ly AS INTEGER)
    DIM r AS INTEGER
    r = HitTest(lx, ly)
    IF r < 0 THEN
      Dismiss()
      RETURN
    END IF
    IF mEn(r) = 0 THEN RETURN
    mSel = r
    Activate()
  END SUB

  SUB MouseOver(lx AS INTEGER, ly AS INTEGER)
    DIM r AS INTEGER
    r = HitTest(lx, ly)
    IF r >= 0 AND mEn(r) = 1 THEN mSel = r
  END SUB

  ' =============== tastiera ===============
  SUB HandleKey(k AS INTEGER)
    IF mOpen = 0 THEN RETURN
    IF k = 38 THEN MoveSel(-1)
    IF k = 40 THEN MoveSel(1)
    IF k = 36 THEN MoveSel(-16)
    IF k = 35 THEN MoveSel(16)
    IF k = 13 THEN Activate()
    IF k = 27 THEN Dismiss()
  END SUB

  ' =============== disegno ===============
  SUB Paint(g AS ANY)
    g.Clear()
    IF mOpen = 0 THEN RETURN
    ' sfondo + bordo
    g.SetColor(mCBg)
    g.FillRect(mX, mY, mW, mN)
    g.SetColor(mCBorder)
    g.Rect(mX, mY, mW - 1, mN - 1)
    DIM i AS INTEGER
    FOR i = 0 TO mN - 1
      DIM ly AS INTEGER
      ly = mY + i
      IF mSep(i) <> 0 THEN
        g.SetColor(mCDis)
        g.Line(mX, ly, mX + mW - 1, ly)
      ELSE
        IF i = mSel AND mEn(i) = 1 THEN
          g.SetColor(mCSelBg)
          g.FillRect(mX, ly, mW, 1)
          g.SetColor(mCSelFg)
          g.Text(mItems(i), mX + 1, ly)
        ELSE
          IF mEn(i) = 1 THEN g.SetColor(mCFg) ELSE g.SetColor(mCDis)
          g.Text(mItems(i), mX + 1, ly)
        END IF
      END IF
    NEXT i
  END SUB
END CLASS
