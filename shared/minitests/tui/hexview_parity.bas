' Minitest TUI HEXVIEW — parità 4 backend (statement widget TUI).
' EXPECT: OK-hexview
IMPORT CONTROLS
SUB Main()
  DIM v AS INTEGER
  HEXVIEW 7, 5, 30, 6, "ABC" INTO v
  PRINT "OK-hexview"
END SUB
