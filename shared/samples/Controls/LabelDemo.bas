' ============================================================================
' LabelDemo — sample del controllo CONTROLS LABEL (etichetta display-only).
' ============================================================================
' Statement (standalone):
'     LABEL row, col, width, "testo" [INTO var]
'   - row/col: posizione; width: larghezza; testo: stringa.
'   - INTO opzionale (display-only, come SPINNER): senza INTO il valore è scartato.
'   - identità/proprietà/eventi come gli altri controlli (ID, CONTROL PROPS,
'     ON ... CALL/GOSUB).
'
' Headless: il widget è MODALE; il sample inietta SENDKEY CHR(27) per chiuderlo
' sui backend C (canary/C99Native).
'
' Esecuzione:  tools/gmb_c99native_run.sh samples/Controls/LabelDemo.bas
' ============================================================================
IMPORT CONTROLS

SUB Main()
  PRINT "=== LabelDemo ==="
  LABEL 5, 2, 30, "Etichetta di esempio"
  SENDKEY CHR(27)
  PRINT "=== fine ==="
END SUB
