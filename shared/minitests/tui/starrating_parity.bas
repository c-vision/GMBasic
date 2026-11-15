' Minitest TUI STARRATING — parità 4 backend (statement widget TUI).
' EXPECT: OK-starrating
IMPORT CONTROLS
SUB Main()
  DIM v AS INTEGER
  STARRATING 7, 5, 12, 5, 3 INTO v
  PRINT "OK-starrating"
END SUB
