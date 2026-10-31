' ============================================================================
' DataListDemo — sample del controllo CONTROLS DATALIST (lista dati scorrevole).
' ============================================================================
' Statement (standalone):
'     DATALIST row, col, width, height, [elementi...] INTO var
'   - row/col: posizione; width/height: dimensione area.
'   - elementi: array di stringhe (una per voce), come LISTBOX/SCROLLVIEW.
'   - INTO var riceve 0.
'   - identità/proprietà/eventi come gli altri controlli (ID, CONTROL PROPS,
'     ON ... CALL/GOSUB).
'
' Headless: il widget è MODALE; il sample inietta SENDKEY CHR(27) per chiuderlo
' sui backend C (canary/C99Native).
'
' Esecuzione:  tools/gmb_c99native_run.sh samples/Community/Controls/DataListDemo.bas
' ============================================================================
IMPORT CONTROLS

SUB Main()
  PRINT "=== DataListDemo ==="
  DIM v AS INTEGER
  DATALIST 5, 2, 40, 6, ["Alfa", "Beta", "Gamma", "Delta"] INTO v
  SENDKEY CHR(27)
  PRINT "ok=" + LTRIM(STR(v))
  PRINT "=== fine ==="
END SUB
