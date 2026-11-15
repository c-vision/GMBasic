' Minitest TUI DATALIST — parità 4 backend (statement widget TUI).
' EXPECT: OK-datalist
IMPORT CONTROLS
SUB Main()
  DIM v AS INTEGER
  DATALIST 7, 5, 30, 5, ["uno", "due", "tre"] INTO v
  PRINT "OK-datalist"
END SUB
