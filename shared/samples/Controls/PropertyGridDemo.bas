' ============================================================================
' PropertyGridDemo — sample del controllo CONTROLS PROPERTYGRID (griglia nome/valore).
' ============================================================================
' Statement (standalone):
'     PROPERTYGRID row, col, width, height, ["nome","valore",...] INTO var
'   - row/col: posizione; width/height: dimensione area.
'   - elementi: array di stringhe a COPPIE (nome, valore), come le proprietà di una griglia.
'   - INTO var riceve 0.
'   - identità/proprietà/eventi come gli altri controlli (ID, CONTROL PROPS,
'     ON ... CALL/GOSUB).
'
' Headless: il widget è MODALE; il sample inietta SENDKEY CHR(27) per chiuderlo
' sui backend C (canary/C99Native).
'
' Esecuzione:  tools/gmb_c99native_run.sh samples/Community/Controls/PropertyGridDemo.bas
' ============================================================================
IMPORT CONTROLS

SUB Main()
  PRINT "=== PropertyGridDemo ==="
  DIM v AS INTEGER
  PROPERTYGRID 5, 2, 40, 6, ["Nome", "Demo", "Versione", "1.0", "Attivo", "Sì"] INTO v
  SENDKEY CHR(27)
  PRINT "ok=" + LTRIM(STR(v))
  PRINT "=== fine ==="
END SUB
