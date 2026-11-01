' ============================================================================
' FileExplorerDemo — sample del controllo CONTROLS FILEEXPLORER (esplora file).
' ============================================================================
' Statement (standalone):
'     FILEEXPLORER row, col, width, height, "path" INTO var
'   - row/col: posizione; width/height: dimensione; path: cartella iniziale.
'   - INTO var riceve 0 (l'azione/percorso si leggono dalle proprietà del controllo).
'   - identità/proprietà/eventi come gli altri controlli (ID, CONTROL PROPS,
'     ON ... CALL/GOSUB).
'
' Headless: il widget è MODALE; il sample inietta SENDKEY CHR(27) per chiuderlo
' sui backend C (canary/C99Native).
'
' Esecuzione:  tools/gmb_c99native_run.sh samples/Controls/FileExplorerDemo.bas
' ============================================================================
IMPORT CONTROLS

SUB Main()
  PRINT "=== FileExplorerDemo ==="
  DIM v AS INTEGER
  FILEEXPLORER 5, 2, 40, 10, "." INTO v
  SENDKEY CHR(27)
  PRINT "ok=" + LTRIM(STR(v))
  PRINT "=== fine ==="
END SUB
