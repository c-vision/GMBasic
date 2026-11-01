' ============================================================================
' ScrollViewDemo — sample del controllo CONTROLS SCROLLVIEW (area scorrevole).
' ============================================================================
' Statement (standalone):
'     SCROLLVIEW row, col, width, height, [righe...] INTO var
'   - row/col: posizione; width/height: dimensione area.
'   - contenuto: array di stringhe (una per riga), come TAB/GRID.
'   - INTO var riceve 0.
'   - identità/proprietà/eventi come gli altri controlli (ID, CONTROL PROPS,
'     ON ... CALL/GOSUB).
'
' Headless: il widget è MODALE; il sample inietta SENDKEY CHR(27) per chiuderlo
' sui backend C (canary/C99Native).
'
' Esecuzione:  tools/gmb_c99native_run.sh samples/Controls/ScrollViewDemo.bas
' ============================================================================
IMPORT CONTROLS

SUB Main()
  PRINT "=== ScrollViewDemo ==="
  DIM v AS INTEGER
  SCROLLVIEW 5, 2, 40, 6, ["Intestazione", "riga uno", "riga due", "riga tre"] INTO v
  SENDKEY CHR(27)
  PRINT "ok=" + LTRIM(STR(v))
  PRINT "=== fine ==="
END SUB
