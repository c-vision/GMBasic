' Minitest TUI TAB — parità 4 backend (statement widget TUI).
' EXPECT: OK-tab
IMPORT CONTROLS
SUB Main()
  DIM v AS INTEGER
  TAB 9, 5, 50, 3, ["A", "B", "C"] INTO v
  PRINT "OK-tab"
END SUB
