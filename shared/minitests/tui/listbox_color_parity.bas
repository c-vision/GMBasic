' Minitest TUI LISTBOX + colore Clipper (spec "W+/B") — parità 4 backend.
' EXPECT: OK-listbox-color
IMPORT CONTROLS
SUB Main()
  DIM v AS INTEGER
  LISTBOX 5, 2, 30, 5, ["uno", "due", "tre"] INTO v STYLE "color=W+/B"
  SENDKEY CHR(27)
  PRINT "OK-listbox-color"
END SUB
