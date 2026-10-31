' Watchlist.bas — Watchlist (tabella simboli/prezzi) come UserControl (§12.4).
' Tabella di simboli con ultimo prezzo e variazione %; aggiornamento tick, selezione
' via hit-test, rimozione riga. Componente di dominio trading → classe BASIC
' INHERITS Control (parità 4 backend per costruzione).
'
' Il controllo è AGNOSTICO dal trasporto: i prezzi arrivano con `ApplyTick` (in
' GMBSuite il feed è servito via FFI, `gbt_ffi_broker`); qui non si cablano motori.
IMPORT CONTROLS

NAMESPACE CustomControls

PUBLIC CLASS Watchlist INHERITS Control
  PRIVATE DIM mN AS INTEGER
  PRIVATE DIM mSym(64) AS STRING
  PRIVATE DIM mPx(64) AS DOUBLE
  PRIVATE DIM mPrev(64) AS DOUBLE
  PRIVATE DIM mSel AS INTEGER

  SUB Append(sym AS STRING, px AS DOUBLE)
    IF mN >= 64 THEN RETURN
    mSym(mN) = sym : mPx(mN) = px : mPrev(mN) = px
    mN = mN + 1
  END SUB

  FUNCTION Count() AS INTEGER
    RETURN mN
  END FUNCTION

  FUNCTION SymbolOf(i AS INTEGER) AS STRING
    RETURN mSym(i)
  END FUNCTION

  FUNCTION PriceOf(i AS INTEGER) AS DOUBLE
    RETURN mPx(i)
  END FUNCTION

  ' Variazione % rispetto al prezzo precedente.
  FUNCTION ChangePct(i AS INTEGER) AS DOUBLE
    IF mPrev(i) = 0.0 THEN RETURN 0.0
    RETURN (mPx(i) - mPrev(i)) / mPrev(i) * 100.0
  END FUNCTION

  FUNCTION IndexOf(sym AS STRING) AS INTEGER
    DIM i AS INTEGER
    FOR i = 0 TO mN - 1
      IF mSym(i) = sym THEN RETURN i
    NEXT i
    RETURN -1
  END FUNCTION

  ' Tick di mercato: aggiorna il prezzo (il vecchio diventa il riferimento della variazione).
  SUB ApplyTick(sym AS STRING, px AS DOUBLE)
    DIM i AS INTEGER
    i = IndexOf(sym)
    IF i < 0 THEN
      Append(sym, px)
    ELSE
      IF px <> mPx(i) THEN
        mPrev(i) = mPx(i)
        mPx(i) = px
      END IF
    END IF
  END SUB

  SUB RemoveRow(i AS INTEGER)
    DIM k AS INTEGER
    IF i < 0 OR i >= mN THEN RETURN
    FOR k = i TO mN - 2
      mSym(k) = mSym(k + 1)
      mPx(k) = mPx(k + 1)
      mPrev(k) = mPrev(k + 1)
    NEXT k
    mN = mN - 1
    IF mSel >= mN THEN mSel = mN - 1
  END SUB

  ' Hit-test: una riga è alta 1 cella → riga = ly. Seleziona e ritorna la riga (-1 fuori).
  FUNCTION Hit(lx AS INTEGER, ly AS INTEGER) AS INTEGER
    IF ly < 0 OR ly >= mN THEN RETURN -1
    mSel = ly
    RETURN ly
  END FUNCTION

  FUNCTION SelectedRow() AS INTEGER
    RETURN mSel
  END FUNCTION

  SUB Paint(g AS ANY)
    g.Clear()
    g.SetColor(8421504)
    g.Rect(0, 0, 30, 12)
    DIM i AS INTEGER
    DIM ch AS DOUBLE
    FOR i = 0 TO mN - 1
      ch = ChangePct(i)
      IF ch < 0.0 THEN
        g.SetColor(16711680)
      ELSE
        g.SetColor(65280)
      END IF
      g.Text(mSym(i), 1, i)
      g.Text(STR(mPx(i)) + " " + STR(INT(ch * 100.0 + 0.5) / 100.0) + "%", 13, i)
      IF i = mSel THEN
        g.SetColor(16777215)
        g.Rect(0, i, 30, 1)
      END IF
    NEXT i
  END SUB
END CLASS
