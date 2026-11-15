' Minitest TUI COLORPICKER — parità 4 backend (statement widget TUI).
' EXPECT: OK-colorpicker
IMPORT CONTROLS
SUB Main()
  DIM v AS INTEGER
  COLORPICKER 26, 5, 16 INTO v
  PRINT "OK-colorpicker"
END SUB
