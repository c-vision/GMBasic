' ============================================================================
' NumericUpDownDemo — sample del controllo CONTROLS NUMERICUPDOWN (spin-box).
' ============================================================================
' Statement (standalone):
'     NUMERICUPDOWN row, col, length, min, max, step INTO var
'   - row/col: posizione; length: larghezza campo; min/max: range; step: passo.
'   - INTO var riceve il valore (su chiusura = min, come SLIDER).
'   - identità/proprietà/eventi come gli altri controlli (ID, CONTROL PROPS,
'     ON ... CALL/GOSUB).
'
' Retro-compat Clipper: FORMAT/VALID ecc. sono gestiti dalle proprietà control
' (CONTROL APPLY PROPS) — vedi ControlCenter.bas per l'uso combinato.
'
' Headless: il widget è MODALE; il sample inietta SENDKEY CHR(27) prima/dopo
' per chiuderlo sui backend C (canary/C99Native).
'
' Esecuzione:  tools/gmb_c99native_run.sh samples/Controls/NumericUpDownDemo.bas
' ============================================================================
IMPORT CONTROLS

SUB Main()
  PRINT "=== NumericUpDownDemo ==="
  DIM v AS INTEGER
  NUMERICUPDOWN 5, 2, 12, 0, 100, 5 INTO v
  SENDKEY CHR(27)
  PRINT "valore=" + LTRIM(STR(v))
  PRINT "=== fine ==="
END SUB
