' Splitter.bas — Splitter (divisore a due pannelli) come UserControl (§12.4).
' Divide un'area in due pannelli con una barra divisoria TRASCINABILE. Il controllo
' gestisce la geometria (barra + rettangoli dei pannelli) e il disegno; l'host legge i
' rettangoli (PaneA*/PaneB*) per posizionarvi eventuali controlli figli (PARENT).
' Componente di dominio/UI → classe BASIC INHERITS Control (parità 4 backend).
IMPORT CONTROLS

NAMESPACE CustomControls

PUBLIC CLASS Splitter INHERITS Control
  PRIVATE DIM mOrient AS INTEGER       ' 0 = verticale (A|B affiancati), 1 = orizzontale (A/B sopra-sotto)
  PRIVATE DIM mTotal AS INTEGER        ' dimensione lungo l'asse (larghezza o altezza), in celle
  PRIVATE DIM mCross AS INTEGER        ' dimensione trasversale (altezza o larghezza), in celle
  PRIVATE DIM mThick AS INTEGER        ' spessore della barra divisoria
  PRIVATE DIM mPos AS INTEGER          ' posizione della barra (colonna/riga d'inizio della barra)
  PRIVATE DIM mMinA AS INTEGER
  PRIVATE DIM mMinB AS INTEGER
  PRIVATE DIM mDragging AS INTEGER
  PRIVATE DIM mHover AS INTEGER
  PRIVATE DIM mCBgA AS DOUBLE
  PRIVATE DIM mCBgB AS DOUBLE
  PRIVATE DIM mCDiv AS DOUBLE
  PRIVATE DIM mCDivHot AS DOUBLE
  PRIVATE DIM mCBorder AS DOUBLE

  ' =============== configurazione ===============
  ' orient: 0=verticale, 1=orizzontale. total: lunghezza dell'asse. thick: spessore barra.
  ' cross: dimensione trasversale (per il disegno dei pannelli).
  SUB Configure(orient AS INTEGER, total AS INTEGER, thick AS INTEGER, cross AS INTEGER)
    mOrient = orient : mTotal = total : mThick = thick : mCross = cross
    mMinA = 3 : mMinB = 3
    mDragging = 0 : mHover = 0
    ' NB: calcolo in un DOUBLE locale poi INT (l'IL sbaglia `INT((a-b)/2)` inline su campo).
    DIM half AS DOUBLE
    half = (total - thick) / 2
    mPos = INT(half)
    mCBgA = 1973790
    mCBgB = 1250067
    mCDiv = 8421504
    mCDivHot = 16776960
    mCBorder = 4210752
  END SUB

  SUB SetTheme(bgA AS DOUBLE, bgB AS DOUBLE, divCol AS DOUBLE, divHot AS DOUBLE, borderCol AS DOUBLE)
    mCBgA = bgA : mCBgB = bgB : mCDiv = divCol : mCDivHot = divHot : mCBorder = borderCol
  END SUB

  ' =============== letture ===============
  FUNCTION Orient() AS INTEGER
    RETURN mOrient
  END FUNCTION
  FUNCTION Pos() AS INTEGER
    RETURN mPos
  END FUNCTION
  FUNCTION Thick() AS INTEGER
    RETURN mThick
  END FUNCTION
  FUNCTION Dragging() AS INTEGER
    RETURN mDragging
  END FUNCTION

  ' =============== geometria (rettangoli dei pannelli, in celle locali) ===============
  FUNCTION PaneAX() AS INTEGER
    IF mOrient = 0 THEN RETURN 0
    RETURN 0
  END FUNCTION
  FUNCTION PaneAY() AS INTEGER
    IF mOrient = 0 THEN RETURN 0
    RETURN 0
  END FUNCTION
  FUNCTION PaneAW() AS INTEGER
    IF mOrient = 0 THEN RETURN mPos
    RETURN mCross
  END FUNCTION
  FUNCTION PaneAH() AS INTEGER
    IF mOrient = 0 THEN RETURN mCross
    RETURN mPos
  END FUNCTION

  FUNCTION PaneBX() AS INTEGER
    IF mOrient = 0 THEN RETURN mPos + mThick
    RETURN 0
  END FUNCTION
  FUNCTION PaneBY() AS INTEGER
    IF mOrient = 0 THEN RETURN 0
    RETURN mPos + mThick
  END FUNCTION
  FUNCTION PaneBW() AS INTEGER
    IF mOrient = 0 THEN RETURN mTotal - mPos - mThick
    RETURN mCross
  END FUNCTION
  FUNCTION PaneBH() AS INTEGER
    IF mOrient = 0 THEN RETURN mCross
    RETURN mTotal - mPos - mThick
  END FUNCTION

  ' =============== spostamento barra ===============
  SUB SetPos(p AS INTEGER)
    mPos = p
    ClampPos()
  END SUB

  SUB ClampPos()
    IF mPos < mMinA THEN mPos = mMinA
    DIM maxp AS INTEGER
    maxp = mTotal - mThick - mMinB
    IF maxp < mMinA THEN maxp = mMinA
    IF mPos > maxp THEN mPos = maxp
  END SUB

  ' =============== hit-test ===============
  ' Ritorna 0=pannello A, 1=barra, 2=pannello B (-1 se fuori dall'asse).
  FUNCTION HitTest(lx AS INTEGER, ly AS INTEGER) AS INTEGER
    DIM a AS INTEGER
    IF mOrient = 0 THEN a = lx ELSE a = ly
    IF a < 0 OR a > mTotal THEN RETURN -1
    IF a >= mPos AND a < mPos + mThick THEN RETURN 1
    IF a < mPos THEN RETURN 0
    RETURN 2
  END FUNCTION

  ' =============== interazione ===============
  SUB MousePress(lx AS INTEGER, ly AS INTEGER)
    DIM h AS INTEGER
    h = HitTest(lx, ly)
    IF h = 1 THEN mDragging = 1
  END SUB

  SUB MouseDrag(lx AS INTEGER, ly AS INTEGER)
    mHover = HitTest(lx, ly)
    IF mDragging = 0 THEN RETURN
    IF mOrient = 0 THEN
      SetPos(lx - INT(mThick / 2))
    ELSE
      SetPos(ly - INT(mThick / 2))
    END IF
  END SUB

  SUB MouseRelease()
    mDragging = 0
  END SUB

  ' =============== disegno ===============
  SUB Paint(g AS ANY)
    g.Clear()
    ' pannello A
    g.SetColor(mCBgA)
    g.FillRect(PaneAX(), PaneAY(), PaneAW(), PaneAH())
    ' pannello B
    g.SetColor(mCBgB)
    g.FillRect(PaneBX(), PaneBY(), PaneBW(), PaneBH())
    ' barra divisoria (evidenziata se dragging/hover)
    DIM dc AS DOUBLE
    dc = mCDiv
    IF mDragging <> 0 OR mHover = 1 THEN dc = mCDivHot
    g.SetColor(dc)
    IF mOrient = 0 THEN
      g.FillRect(mPos, 0, mThick, mCross)
      ' grip centrale
      g.SetColor(mCBorder)
      g.FillRect(mPos, INT(mCross / 2) - 1, mThick, 2)
    ELSE
      g.FillRect(0, mPos, mCross, mThick)
      g.SetColor(mCBorder)
      g.FillRect(INT(mCross / 2) - 1, mPos, 2, mThick)
    END IF
    ' bordo
    g.SetColor(mCBorder)
    g.Rect(0, 0, mTotal - 1, mCross - 1)
  END SUB
END CLASS
