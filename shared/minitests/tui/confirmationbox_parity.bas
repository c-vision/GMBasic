' Minitest TUI CONFIRMATIONBOX — parità 4 backend (statement widget TUI).
IMPORT CONTROLS
SUB Main()
  DIM v AS INTEGER
  CONFIRMATIONBOX 5, 2, 30, 6, "Confermi?" INTO v
  SENDKEY CHR(27)
  PRINT "OK-confirmationbox"
END SUB
