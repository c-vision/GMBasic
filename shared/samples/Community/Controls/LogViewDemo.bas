' ============================================================================
' LogViewDemo — sample del controllo CONTROLS LOGVIEW (console di log scorrevole).
' ============================================================================
' Statement (standalone):
'     LOGVIEW row, col, width, height, ["messaggio", ...] INTO var
'   - row/col: posizione; width/height: dimensione area.
'   - record: array di stringhe (ogni voce è un record, livello 0 / target vuoto).
'   - INTO var riceve 0.
'   - identità/proprietà/eventi come gli altri controlli (ID, CONTROL PROPS,
'     ON ... CALL/GOSUB).
'
' Headless: il widget è MODALE; il sample inietta SENDKEY CHR(27) per chiuderlo
' sui backend C (canary/C99Native).
'
' Esecuzione:  tools/gmb_c99native_run.sh samples/Community/Controls/LogViewDemo.bas
' ============================================================================
IMPORT CONTROLS

SUB Main()
  PRINT "=== LogViewDemo ==="
  DIM v AS INTEGER
  LOGVIEW 5, 2, 40, 6, ["[INFO] avvio", "[INFO] pronto", "[WARN] demo"] INTO v
  SENDKEY CHR(27)
  PRINT "ok=" + LTRIM(STR(v))
  PRINT "=== fine ==="
END SUB
