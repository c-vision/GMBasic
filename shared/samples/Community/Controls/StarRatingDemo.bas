' ============================================================================
' StarRatingDemo — sample del controllo CONTROLS STARRATING (valutazione a stelle).
' ============================================================================
' Statement (standalone):
'     STARRATING row, col, length, max, value INTO var
'   - row/col: posizione; length: larghezza; max: numero di stelle; value: stelle attive.
'   - INTO var riceve il valore corrente.
'   - identità/proprietà/eventi come gli altri controlli (ID, CONTROL PROPS,
'     ON ... CALL/GOSUB).
'
' Headless: il widget è MODALE; il sample inietta SENDKEY CHR(27) per chiuderlo
' sui backend C (canary/C99Native).
'
' Esecuzione:  tools/gmb_c99native_run.sh samples/Community/Controls/StarRatingDemo.bas
' ============================================================================
IMPORT CONTROLS

SUB Main()
  PRINT "=== StarRatingDemo ==="
  DIM v AS INTEGER
  STARRATING 5, 2, 12, 5, 3 INTO v
  SENDKEY CHR(27)
  PRINT "stelle=" + LTRIM(STR(v))
  PRINT "=== fine ==="
END SUB
