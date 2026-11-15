' Minitest TUI SLIDER — parità 4 backend (statement widget TUI).
' EXPECT: OK-slider
IMPORT CONTROLS
SUB Main()
  DIM v AS INTEGER
  SLIDER 7, 5, 40, 0, 100, 5 INTO v
  PRINT "OK-slider"
END SUB
