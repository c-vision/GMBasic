' Minitest TUI REORDERLISTBOX — parità 4 backend (statement widget TUI).
IMPORT CONTROLS
SUB Main()
  DIM v AS INTEGER
  REORDERLISTBOX 7, 5, 30, 6, ["uno", "due", "tre"] INTO v
  PRINT "OK-reorderlistbox"
END SUB
