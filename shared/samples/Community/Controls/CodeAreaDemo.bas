' ============================================================================
' CodeAreaDemo — sample del controllo CONTROLS CODEAREA (editor di codice).
' ============================================================================
' Statement (standalone):
'     CODEAREA row, col, width, height, ["riga1", ...] INTO var
'   - row/col: posizione; width/height: dimensione area.
'   - righe: array di stringhe (una per riga di codice), come SCROLLVIEW.
'   - INTO var riceve 0.
'   - identità/proprietà/eventi come gli altri controlli (ID, CONTROL PROPS,
'     ON ... CALL/GOSUB).
'
' Headless: il widget è MODALE; il sample inietta SENDKEY CHR(27) per chiuderlo
' sui backend C (canary/C99Native).
'
' Esecuzione:  tools/gmb_c99native_run.sh samples/Community/Controls/CodeAreaDemo.bas
' ============================================================================
IMPORT CONTROLS

SUB Main()
  PRINT "=== CodeAreaDemo ==="
  DIM v AS INTEGER
  CODEAREA 5, 2, 40, 8, ["SUB Main()", "  x = 1 + 2", "  PRINT x", "END SUB"] INTO v
  SENDKEY CHR(27)
  PRINT "ok=" + LTRIM(STR(v))
  PRINT "=== fine ==="
END SUB
