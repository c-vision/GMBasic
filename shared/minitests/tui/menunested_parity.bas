' Minitest TUI MENUNESTED — parità 4 backend (statement widget TUI).
' EXPECT: OK-menunested
IMPORT CONTROLS
SUB Main()
  DIM v AS INTEGER
  MENU 22, 5, [["File", "New", "Open"], ["Edit", "Cut"]] INTO v
  PRINT "OK-menunested"
END SUB
