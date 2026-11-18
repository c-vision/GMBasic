' Minitest TUI NUMERICUPDOWN — parità 4 backend (statement widget TUI).
' EXPECT: OK-numericupdown
IMPORT CONTROLS
SUB Main()
  DIM v AS INTEGER
  NUMERICUPDOWN 7, 5, 40, 0, 100, 5 INTO v
  PRINT "OK-numericupdown"
END SUB
