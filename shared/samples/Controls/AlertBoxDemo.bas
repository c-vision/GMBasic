' ============================================================================
' AlertBoxDemo — sample del controllo CONTROLS ALERTBOX (avviso modale).
' ============================================================================
' Statement (standalone):
'     ALERTBOX row, col, width, height, "messaggio" [INTO var]
'   - row/col: posizione; width/height: dimensione; messaggio: testo mostrato.
'   - INTO opzionale (display-only).
'   - identità/proprietà/eventi come gli altri controlli (ID, CONTROL PROPS,
'     ON ... CALL/GOSUB).
'
' Headless: il widget è MODALE; il sample inietta SENDKEY CHR(27) per chiuderlo
' sui backend C (canary/C99Native).
'
' Esecuzione:  tools/gmb_c99native_run.sh samples/Community/Controls/AlertBoxDemo.bas
' ============================================================================
IMPORT CONTROLS

SUB Main()
  PRINT "=== AlertBoxDemo ==="
  ALERTBOX 5, 2, 40, 8, "Operazione completata."
  SENDKEY CHR(27)
  PRINT "=== fine ==="
END SUB
