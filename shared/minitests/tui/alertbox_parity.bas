' Minitest TUI ALERTBOX — parità 4 backend (statement widget TUI, display-only).
IMPORT CONTROLS
SUB Main()
  ALERTBOX 5, 2, 30, 6, "Attenzione"
  SENDKEY CHR(27)
  PRINT "OK-alertbox"
END SUB
