' Minitest TUI SCROLLBAR — parità 4 backend (statement widget TUI).
' EXPECT: OK-scrollbar
IMPORT CONTROLS
SUB Main()
  DIM v AS INTEGER
  SCROLLBAR 7, 5, 20, 0, 100, 25 INTO v
  PRINT "OK-scrollbar"
END SUB
