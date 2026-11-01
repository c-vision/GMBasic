' ============================================================================
' ScrollBarDemo — sample del controllo CONTROLS SCROLLBAR (barra di scorrimento).
' ============================================================================
' Statement (standalone):
'     SCROLLBAR row, col, length, min, max, value INTO var
'   - row/col: posizione; length: lunghezza barra; min/max: range; value: posizione.
'   - INTO var riceve la posizione corrente.
'   - identità/proprietà/eventi come gli altri controlli (ID, CONTROL PROPS,
'     ON ... CALL/GOSUB). L'orientamento è una proprietà del controllo.
'
' Headless: il widget è MODALE; il sample inietta SENDKEY CHR(27) per chiuderlo
' sui backend C (canary/C99Native).
'
' Esecuzione:  tools/gmb_c99native_run.sh samples/Controls/ScrollBarDemo.bas
' ============================================================================
IMPORT CONTROLS

SUB Main()
  PRINT "=== ScrollBarDemo ==="
  DIM v AS INTEGER
  SCROLLBAR 5, 2, 30, 0, 100, 25 INTO v
  SENDKEY CHR(27)
  PRINT "posizione=" + LTRIM(STR(v))
  PRINT "=== fine ==="
END SUB
