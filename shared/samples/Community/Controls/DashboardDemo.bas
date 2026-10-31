' ============================================================================
' DashboardDemo — sample del controllo CONTROLS DASHBOARD (metriche).
' ============================================================================
' Statement (standalone):
'     DASHBOARD row, col, width, height, ["etichetta","valore","max","unita",...] INTO var
'   - row/col: posizione; width/height: dimensione.
'   - metriche: gruppi di 4 stringhe (etichetta, valore, max, unita').
'   - INTO var riceve 0.
'   - identità/proprietà/eventi come gli altri controlli (ID, CONTROL PROPS,
'     ON ... CALL/GOSUB).
'
' Headless: il widget è MODALE; il sample inietta SENDKEY CHR(27) per chiuderlo
' sui backend C (canary/C99Native).
'
' Esecuzione:  tools/gmb_c99native_run.sh samples/Community/Controls/DashboardDemo.bas
' ============================================================================
IMPORT CONTROLS

SUB Main()
  PRINT "=== DashboardDemo ==="
  DIM v AS INTEGER
  DASHBOARD 5, 2, 40, 8, ["CPU", "42", "100", "%", "RAM", "7", "16", "GB"] INTO v
  SENDKEY CHR(27)
  PRINT "ok=" + LTRIM(STR(v))
  PRINT "=== fine ==="
END SUB
