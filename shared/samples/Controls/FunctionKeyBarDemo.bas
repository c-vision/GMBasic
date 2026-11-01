' ============================================================================
' FunctionKeyBarDemo — sample del controllo CONTROLS FUNCTIONKEYBAR
' (barra dei tasti funzione: una riga con F1..F10 e relativa etichetta).
' ============================================================================
' Statement (standalone, ramo pixel):
'     FUNCTIONKEYBAR row, col, width [, "1Help|2Save|..."]
'   - row/col: posizione; width: larghezza della barra.
'   - 4° argomento opzionale: etichette separate da '|' (max 10, nella forma
'     "<tasto><testo>", es. "1Help"). Se assente valgono i default
'     (1Help 2Save 3Load 4Find 5Run 6Edit 7List 8Auto 9Del 10Quit).
'   - identità/proprietà/eventi come gli altri controlli (NAME, CONTROL SET/GET,
'     ON ... CALL/GOSUB): la barra rende a CELLE in TEXT e come barra pixel in GRAPHICS.
'
' Headless: il widget è MODALE; il sample inietta SENDKEY CHR(27) per chiuderlo
' sui backend C (canary/C99Native).
'
' Esecuzione:  tools/gmb_c99native_run.sh samples/Controls/FunctionKeyBarDemo.bas
' ============================================================================
IMPORT CONTROLS

SUB Main()
  PRINT "=== FunctionKeyBarDemo ==="

  ' Etichette di default (nessun 4° argomento).
  FUNCTIONKEYBAR 1, 1, 70
  SENDKEY CHR(27)

  ' Etichette personalizzate.
  FUNCTIONKEYBAR 3, 1, 60, "1Apri|2Salva|3Chiudi|4Cerca|5Esegui"
  SENDKEY CHR(27)

  ' Con nome logico (per CONTROL ... / eventi).
  FUNCTIONKEYBAR 5, 1, 50, "1Uno|2Due|3Tre" NAME "fkb"
  SENDKEY CHR(27)

  PRINT "=== fine ==="
END SUB
