' Segmented.bas — controllo "Segmented / toggle-group" (es. selettore timeframe) come UserControl.
' ---------------------------------------------------------------------------
' Veicolo = UserControl (classe BASIC INHERITS Control), NON built-in: nessun verbo/TokenKind
' nuovo, usa solo la base `Control` + le primitive del GraphicsContext. Parità 4 backend.
'
' Un segmented è una fila (orizzontale o verticale) di segmenti mutuamente esclusivi:
' esattamente uno è selezionato. Uso tipico: timeframe 1m / 5m / 15m / 1h.
IMPORT CONTROLS

NAMESPACE CustomControls

PUBLIC CLASS Segmented INHERITS Control
  PRIVATE DIM mItems(16) AS STRING
  PRIVATE DIM mN AS INTEGER
  PRIVATE DIM mSel AS INTEGER
  PRIVATE DIM mVertical AS INTEGER
  PRIVATE DIM mOriginX AS INTEGER
  PRIVATE DIM mOriginY AS INTEGER
  PRIVATE DIM mW AS INTEGER
  PRIVATE DIM mH AS INTEGER

  SUB Configure(vertical AS INTEGER)
    mVertical = vertical
    mSel = 0
  END SUB

  SUB AddItem(t AS STRING)
    IF mN < 16 THEN
      mItems(mN) = t
      mN = mN + 1
    END IF
  END SUB

  FUNCTION Count() AS INTEGER
    RETURN mN
  END FUNCTION

  SUB SetOrigin(ox AS INTEGER, oy AS INTEGER, ww AS INTEGER, hh AS INTEGER)
    mOriginX = ox : mOriginY = oy : mW = ww : mH = hh
  END SUB

  SUB SetSelected(i AS INTEGER)
    IF i >= 0 AND i <= mN - 1 THEN mSel = i
  END SUB

  FUNCTION Selected() AS INTEGER
    RETURN mSel
  END FUNCTION

  FUNCTION SelectedText() AS STRING
    IF mSel < 0 OR mSel > mN - 1 THEN RETURN ""
    RETURN mItems(mSel)
  END FUNCTION

  ' Sposta la selezione di d posti (ciclica).
  SUB MoveSel(d AS INTEGER)
    IF mN <= 0 THEN RETURN
    DIM s AS INTEGER
    s = mSel + d
    IF s < 0 THEN s = s + mN
    IF s > mN - 1 THEN s = s - mN
    mSel = s
  END SUB

  ' Indice del segmento alle coordinate locali; -1 se fuori dal controllo.
  FUNCTION SegmentAt(lx AS INTEGER, ly AS INTEGER) AS INTEGER
    IF mN <= 0 THEN RETURN -1
    IF mW < 1 OR mH < 1 THEN RETURN -1
    IF lx < 0 OR ly < 0 THEN RETURN -1
    IF lx >= mW OR ly >= mH THEN RETURN -1
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

  ' Click: seleziona il segmento sotto il cursore. Ritorna l'indice (-1 se fuori).
  FUNCTION Hit(lx AS INTEGER, ly AS INTEGER) AS INTEGER
    DIM i AS INTEGER
    i = SegmentAt(lx, ly)
    IF i >= 0 THEN mSel = i
    RETURN i
  END FUNCTION

  SUB Paint(g AS ANY)
    g.Clear()
    IF mN <= 0 OR mW < 1 OR mH < 1 THEN RETURN
    DIM i AS INTEGER
    IF mVertical <> 0 THEN
      DIM sh AS DOUBLE
      sh = mH : sh = sh / mN
      DIM sj AS DOUBLE
      sj = sh / 2.0
      FOR i = 0 TO mN - 1
        DIM y0 AS DOUBLE
        y0 = i * sh
        IF i = mSel THEN
          g.SetColor(4473924)
          g.FillRect(0, INT(y0), mW, INT(sh))
        END IF
        g.SetColor(8421504)
        g.Rect(0, INT(y0), mW - 1, INT(sh))
        IF i = mSel THEN g.SetColor(16777215)
        IF i <> mSel THEN g.SetColor(12632256)
        DIM lblv AS STRING
        lblv = mItems(i)
        g.Text(lblv, 1, INT(y0 + sj))
      NEXT i
    ELSE
      DIM sw AS DOUBLE
      sw = mW : sw = sw / mN
      DIM sy AS DOUBLE
      sy = mH / 2.0
      FOR i = 0 TO mN - 1
        DIM x0 AS DOUBLE
        x0 = i * sw
        IF i = mSel THEN
          g.SetColor(4473924)
          g.FillRect(INT(x0), 0, INT(sw), mH)
        END IF
        g.SetColor(8421504)
        g.Rect(INT(x0), 0, INT(sw), mH - 1)
        IF i = mSel THEN g.SetColor(16777215)
        IF i <> mSel THEN g.SetColor(12632256)
        DIM lblh AS STRING
        lblh = mItems(i)
        g.Text(lblh, INT(x0) + 1, INT(sy))
      NEXT i
    END IF
  END SUB
END CLASS
