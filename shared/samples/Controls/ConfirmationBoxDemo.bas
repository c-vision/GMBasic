' ============================================================================
' ConfirmationBoxDemo — sample del controllo CONTROLS CONFIRMATIONBOX.
' ============================================================================
' Statement (standalone):
'     CONFIRMATIONBOX row, col, width, height, "messaggio" INTO var
'   - row/col: posizione; width/height: dimensione; messaggio: testo.
'   - INTO var riceve la scelta risultante (-1 non risolta; altrimenti ConfirmationChoice).
'   - identità/proprietà/eventi come gli altri controlli (ID, CONTROL PROPS,
'     ON ... CALL/GOSUB).
'
' Headless: il widget è MODALE; il sample inietta SENDKEY CHR(27) per chiuderlo
' sui backend C (canary/C99Native).
'
' Esecuzione:  tools/gmb_c99native_run.sh samples/Controls/ConfirmationBoxDemo.bas
' ============================================================================
IMPORT CONTROLS

SUB Main()
  PRINT "=== ConfirmationBoxDemo ==="
  DIM v AS INTEGER
  CONFIRMATIONBOX 5, 2, 40, 8, "Procedere con l'operazione?" INTO v
  SENDKEY CHR(27)
  PRINT "scelta=" + LTRIM(STR(v))
  PRINT "=== fine ==="
END SUB
