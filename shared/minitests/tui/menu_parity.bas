' Minitest TUI MENU — parità 4 backend (statement widget TUI).
' EXPECT: OK-menu
IMPORT CONTROLS
SUB Main()
  DIM v AS INTEGER
  MENU 21, 5, ["File", "Edit", "Help"] INTO v
  PRINT "OK-menu"
END SUB
