' Minitest TUI LISTBOX + clausola inline COLOR "W+/B" — parità 4 backend.
' EXPECT: OK-listbox-color-clause
IMPORT CONTROLS
SUB Main()
  DIM v AS INTEGER
  LISTBOX 5, 2, 30, 5, ["uno", "due", "tre"] INTO v COLOR "W+/B"
  SENDKEY CHR(27)
  PRINT "OK-listbox-color-clause"
END SUB
