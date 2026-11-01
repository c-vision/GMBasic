' ============================================================================
' ReorderListBoxDemo — sample del controllo CONTROLS REORDERLISTBOX (lista riordinabile).
' ============================================================================
' Statement (standalone):
'     REORDERLISTBOX row, col, width, height, [elementi...] INTO var
'   - row/col: posizione; width/height: dimensione area.
'   - elementi: array di stringhe (una per voce).
'   - INTO var riceve 0.
'   - identità/proprietà/eventi come gli altri controlli (ID, CONTROL PROPS,
'     ON ... CALL/GOSUB).
'
' Headless: il widget è MODALE; il sample inietta SENDKEY CHR(27) per chiuderlo
' sui backend C (canary/C99Native).
'
' Esecuzione:  tools/gmb_c99native_run.sh samples/Controls/ReorderListBoxDemo.bas
' ============================================================================
IMPORT CONTROLS

SUB Main()
  PRINT "=== ReorderListBoxDemo ==="
  DIM v AS INTEGER
  REORDERLISTBOX 5, 2, 40, 8, ["Primo", "Secondo", "Terzo", "Quarto"] INTO v
  SENDKEY CHR(27)
  PRINT "ok=" + LTRIM(STR(v))
  PRINT "=== fine ==="
END SUB
