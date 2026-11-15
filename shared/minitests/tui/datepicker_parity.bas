' Minitest TUI DATEPICKER — parità 4 backend (statement widget TUI).
' EXPECT: OK-datepicker
IMPORT CONTROLS
SUB Main()
  DIM v AS LONG
  DATEPICKER 24, 5 INTO v
  PRINT "OK-datepicker"
END SUB
