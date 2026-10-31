' ============================================================================
' HexViewDemo — sample del controllo CONTROLS HEXVIEW (dump esadecimale).
' ============================================================================
' Statement (standalone):
'     HEXVIEW row, col, width, height, data$ INTO var
'   - row/col: posizione; width/height: dimensione area.
'   - data$: stringa i cui BYTE vengono mostrati in esadecimale (len = suoi byte).
'   - INTO var riceve 0.
'   - identità/proprietà/eventi come gli altri controlli (ID, CONTROL PROPS,
'     ON ... CALL/GOSUB).
'
' Headless: il widget è MODALE; il sample inietta SENDKEY CHR(27) per chiuderlo
' sui backend C (canary/C99Native).
'
' Esecuzione:  tools/gmb_c99native_run.sh samples/Community/Controls/HexViewDemo.bas
' ============================================================================
IMPORT CONTROLS

SUB Main()
  PRINT "=== HexViewDemo ==="
  DIM v AS INTEGER
  DIM d AS STRING
  d = "GMBasic hex dump!"
  HEXVIEW 5, 2, 40, 6, d INTO v
  SENDKEY CHR(27)
  PRINT "ok=" + LTRIM(STR(v))
  PRINT "=== fine ==="
END SUB
