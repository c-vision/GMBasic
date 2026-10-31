' DataGrid.bas — DataGrid AVANZATO e PERSONALIZZABILE come UserControl (§12.4).
' ---------------------------------------------------------------------------
' Esempio didattico di un controllo dati ricco scritto INTERAMENTE in BASIC
' (`CLASS ... INHERITS Control`), senza alcun verbo/builtin nuovo: usa solo la base
' `Control` + le primitive del GraphicsContext. Gira su tutti e 5 i backend.
'
' Funzioni:
'   • colonne configurabili (header, larghezza, allineamento, tipo, editabile)
'   • dati in celle; filtro testo; ordinamento per colonna (asc/desc)
'   • cursore + navigazione da tastiera (frecce/PgUp/PgDn/Home/End)
'   • scrolling orizzontale/verticale (auto con il cursore)
'   • selezione mouse (hit-test) e resize colonne col trascinamento del bordo
'   • editing cella (DblClick/Invio → modifica testo)
'   • zebra + tema colori
'   • export CSV della vista corrente
'   • PERSONALIZZAZIONE via override di metodi VIRTUAL (CellText/CellFg/CellBg/OnChanged)
'
' Esempio di personalizzazione (sottoclasse che evidenzia i numeri negativi):
'   CLASS PriceGrid INHERITS DataGrid
'     PUBLIC OVERRIDE FUNCTION CellFg(vr AS INTEGER, c AS INTEGER) AS INTEGER
'       IF c = 2 AND VAL(DataGrid.GetCell(vr, c)) < 0 THEN RETURN 16711680
'       RETURN 16777215
'     END FUNCTION
'   END CLASS
IMPORT CONTROLS

NAMESPACE CustomControls

PUBLIC CLASS DataGrid INHERITS Control
  ' ---------------- modello dati ----------------
  PRIVATE DIM mNCols AS INTEGER
  PRIVATE DIM mNRows AS INTEGER
  PRIVATE DIM mHdr(16) AS STRING
  PRIVATE DIM mW(16) AS INTEGER            ' larghezza in celle
  PRIVATE DIM mAlign(16) AS INTEGER        ' 0=sinistra 1=centro 2=destra
  PRIVATE DIM mType(16) AS INTEGER         ' 0=testo 1=intero 2=decimale
  PRIVATE DIM mEdit(16) AS INTEGER         ' 1 = colonna editabile
  PRIVATE DIM mCells(512) AS STRING        ' flat: riga*mNCols + colonna

  ' ---------------- vista (righe visibili) ----------------
  PRIVATE DIM mView(64) AS INTEGER         ' view-row -> data-row
  PRIVATE DIM mViewN AS INTEGER
  PRIVATE DIM mKey(64) AS STRING           ' chiave di ordinamento per data-row
  PRIVATE DIM mSortCol AS INTEGER
  PRIVATE DIM mSortDesc AS INTEGER
  PRIVATE DIM mFiltTxt AS STRING

  ' ---------------- interazione ----------------
  PRIVATE DIM mTop AS INTEGER
  PRIVATE DIM mLeft AS INTEGER
  PRIVATE DIM mCur AS INTEGER              ' riga cursore (view)
  PRIVATE DIM mCurCol AS INTEGER
  PRIVATE DIM mHdrH AS INTEGER
  PRIVATE DIM mEditing AS INTEGER
  PRIVATE DIM mEditBuf AS STRING
  PRIVATE DIM mResizeCol AS INTEGER
  PRIVATE DIM mOriginX AS INTEGER
  PRIVATE DIM mOriginY AS INTEGER
  PRIVATE DIM mWidth AS INTEGER
  PRIVATE DIM mHeight AS INTEGER

  ' ---------------- stile ----------------
  PRIVATE DIM mZebra AS INTEGER
  PRIVATE DIM mShowHdr AS INTEGER
  PRIVATE DIM mCGrid AS DOUBLE
  PRIVATE DIM mCHead AS DOUBLE
  PRIVATE DIM mCSel AS DOUBLE
  PRIVATE DIM mCZebra AS DOUBLE
  PRIVATE DIM mCEdit AS DOUBLE
  PRIVATE DIM mCEditBg AS DOUBLE
  PRIVATE DIM mColFg(16) AS DOUBLE
  PRIVATE DIM mColBg(16) AS DOUBLE

  ' =============== configurazione ===============
  SUB Configure(cols AS INTEGER, rows AS INTEGER)
    mNCols = cols : mNRows = rows
    mSortCol = -1 : mSortDesc = 0 : mFiltTxt = ""
    mTop = 0 : mLeft = 0 : mCur = 0 : mCurCol = 0
    mHdrH = 1 : mEditing = 0 : mResizeCol = -1
    mZebra = 1 : mShowHdr = 1
    mCGrid = 8421504
    mCHead = 16776960
    mCSel = 33023
    mCZebra = 2236962
    mCEdit = 65280
    mCEditBg = 1973790
    DIM z AS INTEGER
    FOR z = 0 TO 15
      mColFg(z) = 16777215 : mColBg(z) = -1
    NEXT z
    RebuildView()
  END SUB

  ' Colore di default (configurabile) di una colonna: personalizzazione "senza sottoclasse".
  SUB SetColumnColors(c AS INTEGER, fg AS DOUBLE, bg AS DOUBLE)
    mColFg(c) = fg : mColBg(c) = bg
  END SUB

  SUB SetColumn(c AS INTEGER, h AS STRING, width AS INTEGER, align AS INTEGER, coltype AS INTEGER, editable AS INTEGER)
    mHdr(c) = h : mW(c) = width : mAlign(c) = align : mType(c) = coltype : mEdit(c) = editable
  END SUB

  SUB SetCell(r AS INTEGER, c AS INTEGER, v AS STRING)
    mCells(r * mNCols + c) = v
  END SUB

  SUB SetTheme(gridCol AS DOUBLE, headCol AS DOUBLE, selCol AS DOUBLE, zebraCol AS DOUBLE, editCol AS DOUBLE, editBg AS DOUBLE)
    mCGrid = gridCol : mCHead = headCol : mCSel = selCol : mCZebra = zebraCol : mCEdit = editCol : mCEditBg = editBg
  END SUB

  SUB SetOrigin(ox AS INTEGER, oy AS INTEGER, ww AS INTEGER, hh AS INTEGER)
    mOriginX = ox : mOriginY = oy : mWidth = ww : mHeight = hh
  END SUB

  ' =============== letture ===============
  FUNCTION Cols() AS INTEGER
    RETURN mNCols
  END FUNCTION
  FUNCTION Rows() AS INTEGER
    RETURN mNRows
  END FUNCTION
  FUNCTION ViewCount() AS INTEGER
    RETURN mViewN
  END FUNCTION
  FUNCTION CurRow() AS INTEGER
    RETURN mCur
  END FUNCTION
  FUNCTION CurCol() AS INTEGER
    RETURN mCurCol
  END FUNCTION
  FUNCTION Editing() AS INTEGER
    RETURN mEditing
  END FUNCTION
  FUNCTION DataRow(vr AS INTEGER) AS INTEGER
    IF vr < 0 OR vr >= mViewN THEN RETURN -1
    RETURN mView(vr)
  END FUNCTION
  FUNCTION GetCell(vr AS INTEGER, c AS INTEGER) AS STRING
    DIM dr AS INTEGER
    dr = DataRow(vr)
    IF dr < 0 THEN RETURN ""
    RETURN mCells(dr * mNCols + c)
  END FUNCTION

  ' =============== HOOK di personalizzazione (override) ===============
  ' NB: il valore della cella è PASSATO come parametro così una sottoclasse può usarlo
  ' senza chiamare metodi della base (l'emitter IL non consente, da un metodo di
  ' sottoclasse, di invocare metodi della classe base).
  PUBLIC VIRTUAL FUNCTION CellText(vr AS INTEGER, c AS INTEGER, value AS STRING) AS STRING
    RETURN value
  END FUNCTION
  PUBLIC VIRTUAL FUNCTION CellFg(vr AS INTEGER, c AS INTEGER, value AS STRING) AS DOUBLE
    RETURN mColFg(c)
  END FUNCTION
  PUBLIC VIRTUAL FUNCTION CellBg(vr AS INTEGER, c AS INTEGER, value AS STRING) AS DOUBLE
    RETURN mColBg(c)
  END FUNCTION
  PUBLIC VIRTUAL SUB OnCellChanged(vr AS INTEGER, c AS INTEGER, oldv AS STRING, newv AS STRING)
    ' nessuna azione di default
  END SUB

  ' =============== filtro / ordinamento ===============
  SUB SetFilterText(t AS STRING)
    mFiltTxt = t : RebuildView()
  END SUB
  FUNCTION GetFilterText() AS STRING
    RETURN mFiltTxt
  END FUNCTION
  SUB SortBy(c AS INTEGER, desc AS INTEGER)
    mSortCol = c : mSortDesc = desc : RebuildView()
  END SUB
  FUNCTION SortCol() AS INTEGER
    RETURN mSortCol
  END FUNCTION
  FUNCTION SortDesc() AS INTEGER
    RETURN mSortDesc
  END FUNCTION

  ' Ricostruisce la vista: filtro (contains) + ordinamento (insertion stabile).
  SUB RebuildView()
    DIM i AS INTEGER
    DIM j AS INTEGER
    DIM k AS INTEGER
    DIM hit AS INTEGER
    DIM t AS INTEGER
    mViewN = 0
    FOR i = 0 TO mNRows - 1
      hit = 0
      IF LEN(mFiltTxt) = 0 THEN
        hit = 1
      ELSE
        FOR k = 0 TO mNCols - 1
          IF INSTR(UPPER(mCells(i * mNCols + k)), UPPER(mFiltTxt)) > 0 THEN hit = 1
        NEXT k
      END IF
      IF hit <> 0 THEN
        mView(mViewN) = i
        mViewN = mViewN + 1
      END IF
    NEXT i
    ' chiavi di ordinamento (per riga dati)
    FOR i = 0 TO mNRows - 1
      IF mSortCol >= 0 THEN mKey(i) = mCells(i * mNCols + mSortCol)
    NEXT i
    ' ordinamento a bolle sulla vista
    IF mSortCol >= 0 THEN
      FOR i = 0 TO mViewN - 2
        FOR j = 0 TO mViewN - 2 - i
          IF ShouldShift(mView(j), mView(j + 1)) <> 0 THEN
            t = mView(j) : mView(j) = mView(j + 1) : mView(j + 1) = t
          END IF
        NEXT j
      NEXT i
    END IF
    IF mCur >= mViewN THEN mCur = mViewN - 1
    IF mCur < 0 THEN mCur = 0
  END SUB

  ' 1 se la chiave di `x` va DOPO `y` nell'ordine corrente (x shiftato).
  FUNCTION ShouldShift(x AS INTEGER, y AS INTEGER) AS INTEGER
    DIM less AS INTEGER
    less = 0
    IF mType(mSortCol) = 0 THEN
      IF mKey(x) > mKey(y) THEN less = 1
    ELSE
      IF VAL(mKey(x)) > VAL(mKey(y)) THEN less = 1
    END IF
    IF mSortDesc <> 0 THEN
      IF less <> 0 THEN RETURN 0
      IF mKey(x) <> mKey(y) THEN RETURN 1
      RETURN 0
    END IF
    RETURN less
  END FUNCTION

  ' =============== navigazione ===============
  SUB MoveCursor(dr AS INTEGER, dc AS INTEGER)
    mCur = mCur + dr
    mCurCol = mCurCol + dc
    IF mCur < 0 THEN mCur = 0
    IF mCur > mViewN - 1 THEN mCur = mViewN - 1
    IF mCurCol < 0 THEN mCurCol = 0
    IF mCurCol > mNCols - 1 THEN mCurCol = mNCols - 1
    EnsureVisible()
  END SUB

  SUB PageBy(page As INTEGER)
    MoveCursor(page * (mHeight - mHdrH - 1), 0)
  END SUB

  SUB GoHome()
    mCur = 0 : mCurCol = 0 : EnsureVisible()
  END SUB

  SUB GoEnd()
    mCur = mViewN - 1 : mCurCol = mNCols - 1 : EnsureVisible()
  END SUB

  ' Mantiene riga/colonna corrente visibili (scrolling).
  SUB EnsureVisible()
    DIM visible AS INTEGER
    DIM c AS INTEGER
    visible = mHeight - mHdrH
    IF visible < 1 THEN visible = 1
    IF mCur < mTop THEN mTop = mCur
    IF mCur >= mTop + visible THEN mTop = mCur - visible + 1
    IF mCurCol < mLeft THEN mLeft = mCurCol
    ' Scorre orizzontalmente finché la colonna del cursore non è visibile.
    FOR c = mLeft TO mCurCol - 1
      IF ColRight(mCurCol) - ColRight(c) > mWidth - 2 THEN mLeft = c + 1
    NEXT c
    IF mLeft > mCurCol THEN mLeft = mCurCol
  END SUB

  ' Larghezza cumulativa (posizione X) della colonna c dall'origine del viewport.
  FUNCTION ColRight(c AS INTEGER) AS INTEGER
    DIM acc AS INTEGER
    DIM i AS INTEGER
    acc = 0
    FOR i = mLeft TO c
      acc = acc + mW(i)
    NEXT i
    RETURN acc
  END FUNCTION

  FUNCTION ColX(c AS INTEGER) AS INTEGER
    DIM acc AS INTEGER
    DIM i AS INTEGER
    acc = 0
    FOR i = mLeft TO c - 1
      acc = acc + mW(i)
    NEXT i
    RETURN acc
  END FUNCTION

  ' =============== mouse ===============
  ' Hit-test: converte coordinate locali in (riga view, colonna). Ritorna 1 se in cella.
  FUNCTION HitTest(lx AS INTEGER, ly AS INTEGER) AS INTEGER
    DIM vr AS INTEGER
    DIM acc AS INTEGER
    DIM c AS INTEGER
    IF ly < mHdrH THEN
      ' header: cerca il bordo di colonna per il resize
      acc = 0
      mResizeCol = -1
      FOR c = mLeft TO mNCols - 1
        acc = acc + mW(c)
        IF ABS(lx - acc) <= 1 THEN mResizeCol = c
      NEXT c
      RETURN 0
    END IF
    vr = mTop + (ly - mHdrH)
    IF vr < 0 OR vr >= mViewN THEN RETURN 0
    mCur = vr
    acc = 0
    mCurCol = mNCols - 1
    FOR c = mLeft TO mNCols - 1
      acc = acc + mW(c)
      IF lx < acc THEN
        mCurCol = c
        c = mNCols
      END IF
    NEXT c
    RETURN 1
  END FUNCTION

  ' Eventi mouse del controllo (segnaposto): l'host inoltra le coordinate LOCALI con
  ' MousePress/MouseDrag/MouseRelease (l'API di lettura del mouse dipende dall'host).
  SUB MouseDown()
  END SUB

  SUB MouseMove()
  END SUB

  SUB MouseUp()
    mResizeCol = -1
  END SUB

  ' Pressione a coordinate LOCALI (0,0 = angolo del controllo): seleziona cella o
  ' avvia il resize se si è sul bordo di una colonna dell'header.
  SUB MousePress(lx AS INTEGER, ly AS INTEGER)
    DIM r AS INTEGER
    r = HitTest(lx, ly)
    IF mResizeCol >= 0 THEN mEditing = 0
  END SUB

  ' Trascinamento (resize colonna) a coordinata X locale.
  SUB MouseDrag(lx AS INTEGER)
    IF mResizeCol < 0 THEN RETURN
    DIM neww AS INTEGER
    neww = lx - ColX(mResizeCol)
    IF neww >= 3 THEN mW(mResizeCol) = neww
  END SUB

  SUB MouseRelease()
    mResizeCol = -1
  END SUB

  SUB DblClick()
    IF mCurCol >= 0 THEN
      IF mEdit(mCurCol) <> 0 THEN
        mEditing = 1
        mEditBuf = GetCell(mCur, mCurCol)
      END IF
    END IF
  END SUB

  ' =============== tastiera ===============
  ' Evento del controllo (segnaposto): la lettura del codice tasto dipende dall'host.
  ' Per l'uso programmatico/didattico si chiama HandleKey(k) (codici: 37/38/39/40 frecce,
  ' 33/34 PgUp/PgDn, 36/35 Home/End, 13 Invio, 27 Esc).
  SUB KeyDown()
  END SUB

  SUB HandleKey(k AS INTEGER)
    IF mEditing <> 0 THEN
      ' modifica: INVIO conferma, ESC annulla
      IF k = 13 THEN
        DIM oldv AS STRING
        oldv = GetCell(mCur, mCurCol)
        SetCellDataRow(mCur, mCurCol, mEditBuf)
        mEditing = 0
        OnCellChanged(mCur, mCurCol, oldv, mEditBuf)
      ELSE
        IF k = 27 THEN mEditing = 0
      END IF
      RETURN
    END IF
    IF k = 38 THEN MoveCursor(-1, 0)
    IF k = 40 THEN MoveCursor(1, 0)
    IF k = 37 THEN MoveCursor(0, -1)
    IF k = 39 THEN MoveCursor(0, 1)
    IF k = 33 THEN PageBy(-1)
    IF k = 34 THEN PageBy(1)
    IF k = 36 THEN GoHome()
    IF k = 35 THEN GoEnd()
    IF k = 13 THEN DblClick()
  END SUB

  SUB SetCellDataRow(vr AS INTEGER, c AS INTEGER, v AS STRING)
    DIM dr AS INTEGER
    dr = DataRow(vr)
    IF dr < 0 THEN RETURN
    mCells(dr * mNCols + c) = v
    IF mSortCol = c THEN RebuildView()
  END SUB

  ' =============== export ===============
  FUNCTION ExportCsv() AS STRING
    DIM out AS STRING
    DIM i AS INTEGER
    DIM c AS INTEGER
    out = ""
    IF mShowHdr <> 0 THEN
      FOR c = 0 TO mNCols - 1
        IF c > 0 THEN out = out + ","
        out = out + mHdr(c)
      NEXT c
      out = out + CHR(10)
    END IF
    FOR i = 0 TO mViewN - 1
      FOR c = 0 TO mNCols - 1
        IF c > 0 THEN out = out + ","
        out = out + GetCell(i, c)
      NEXT c
      out = out + CHR(10)
    NEXT i
    RETURN out
  END FUNCTION

  ' =============== disegno ===============
  SUB Paint(g AS ANY)
    g.Clear()
    DIM i AS INTEGER
    DIM c AS INTEGER
    DIM ly AS INTEGER
    DIM lx AS INTEGER
    DIM col AS DOUBLE
    DIM xx AS INTEGER
    ' griglia + righe visibili
    DIM visible AS INTEGER
    visible = mHeight - mHdrH
    FOR i = 0 TO visible - 1
      ly = mHdrH + i
      DIM vr AS INTEGER
      vr = mTop + i
      IF vr < mViewN THEN
        ' sfondo zebra / selezione
        IF vr = mCur THEN
          col = mCSel
        ELSE
          IF mZebra <> 0 AND (vr MOD 2) = 1 THEN
            col = mCZebra
          ELSE
            col = 0
          END IF
        END IF
        IF col <> 0 THEN
          g.SetColor(col)
          g.FillRect(0, ly, mWidth, 1)
        END IF
        ' celle
        xx = 0
        FOR c = mLeft TO mNCols - 1
          IF xx < mWidth THEN
            IF (vr = mCur AND c = mCurCol) THEN
              IF mEditing <> 0 THEN
                g.SetColor(mCEditBg)
                g.FillRect(xx, ly, mW(c), 1)
              END IF
            END IF
            DIM bg AS DOUBLE
            bg = CellBg(vr, c, GetCell(vr, c))
            IF bg >= 0 AND vr <> mCur THEN
              g.SetColor(bg)
              g.FillRect(xx, ly, mW(c), 1)
            END IF
            IF mEditing <> 0 AND vr = mCur AND c = mCurCol THEN
              g.SetColor(mCEdit)
              g.Text(PadText(mEditBuf, mW(c), mAlign(c)), xx + 1, ly)
            ELSE
              g.SetColor(CellFg(vr, c, GetCell(vr, c)))
              g.Text(PadText(CellText(vr, c, GetCell(vr, c)), mW(c), mAlign(c)), xx + 1, ly)
            END IF
            xx = xx + mW(c)
          END IF
        NEXT c
      END IF
    NEXT i
    ' header
    IF mShowHdr <> 0 THEN
      g.SetColor(mCHead)
      g.FillRect(0, 0, mWidth, 1)
      g.SetColor(0)
      xx = 0
      FOR c = mLeft TO mNCols - 1
        IF xx < mWidth THEN
          DIM mark AS STRING
          mark = mHdr(c)
          IF mSortCol = c THEN
            IF mSortDesc <> 0 THEN mark = mark + " v" ELSE mark = mark + " ^"
          END IF
          g.Text(PadText(mark, mW(c), mAlign(c)), xx + 1, 0)
          xx = xx + mW(c)
        END IF
      NEXT c
    END IF
    ' bordo
    g.SetColor(mCGrid)
    g.Rect(0, 0, mWidth - 1, mHeight - 1)
  END SUB

  ' Allinea il testo nella larghezza w secondo l'allineamento (tronca se serve).
  FUNCTION PadText(s AS STRING, w AS INTEGER, align AS INTEGER) AS STRING
    DIM t AS STRING
    t = s
    IF LEN(t) > w - 1 THEN
      IF w > 1 THEN t = LEFT(t, w - 1)
    END IF
    DIM pad AS INTEGER
    pad = w - 1 - LEN(t)
    IF pad < 0 THEN pad = 0
    IF align = 2 THEN
      RETURN SPACE(pad) + t
    END IF
    IF align = 1 THEN
      DIM half AS INTEGER
      half = INT(pad / 2)
      RETURN SPACE(half) + t + SPACE(pad - half)
    END IF
    RETURN t + SPACE(pad)
  END FUNCTION
END CLASS

' ---------------------------------------------------------------------------
' Esempio di personalizzazione: sottoclasse che evidenzia in verde/rosso i
' valori della colonna 2 (Variazione) e rende gialle le celle > 100.
' ---------------------------------------------------------------------------
PUBLIC CLASS PnlGrid INHERITS DataGrid
  PUBLIC OVERRIDE FUNCTION CellFg(vr AS INTEGER, c AS INTEGER, value AS STRING) AS DOUBLE
    IF c = 2 THEN
      IF VAL(value) < 0 THEN RETURN 16711680
      RETURN 65280
    END IF
    RETURN 16777215
  END FUNCTION
  PUBLIC OVERRIDE FUNCTION CellBg(vr AS INTEGER, c AS INTEGER, value AS STRING) AS DOUBLE
    IF c = 1 AND VAL(value) > 300 THEN RETURN 6710886
    RETURN -1
  END FUNCTION
END CLASS
