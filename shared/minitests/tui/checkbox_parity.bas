' Minitest TUI CHECKBOX — parità 4 backend (statement widget TUI).
' EXPECT: OK-checkbox
IMPORT CONTROLS
SUB Main()
  DIM v AS BOOLEAN
  CHECKBOX 11, 5, "notify" INTO v
  PRINT "OK-checkbox"
END SUB
