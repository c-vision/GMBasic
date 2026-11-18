' Minitest TUI LISTBOX — parità 4 backend (statement widget TUI).
' EXPECT: OK-listbox
IMPORT CONTROLS
SUB Main()
  DIM v AS INTEGER
  LISTBOX 13, 5, 30, 5, ["A", "B", "C"] INTO v
  PRINT "OK-listbox"
END SUB
