' ============================================================================
' BigTextDemo — sample del controllo CONTROLS BIGTEXT (testo grande).
' ============================================================================
' Statement (standalone):
'     BIGTEXT row, col, "testo" [INTO var]
'   - row/col: posizione; testo: stringa resa in grande.
'   - INTO opzionale (display-only, come LABEL/SPINNER).
'   - identità/proprietà/eventi come gli altri controlli (ID, CONTROL PROPS,
'     ON ... CALL/GOSUB).
'
' Headless: il widget è MODALE; il sample inietta SENDKEY CHR(27) per chiuderlo
' sui backend C (canary/C99Native).
'
' Esecuzione:  tools/gmb_c99native_run.sh samples/Community/Controls/BigTextDemo.bas
' ============================================================================
IMPORT CONTROLS

SUB Main()
  PRINT "=== BigTextDemo ==="
  BIGTEXT 5, 2, "GMB"
  SENDKEY CHR(27)
  PRINT "=== fine ==="
END SUB
