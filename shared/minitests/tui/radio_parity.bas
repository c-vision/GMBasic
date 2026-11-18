' Minitest TUI RADIO — parità 4 backend (statement widget TUI).
' EXPECT: OK-radio
IMPORT CONTROLS
SUB Main()
  DIM v AS INTEGER
  RADIO 5, 5, ["A", "B", "C"] INTO v
  PRINT "OK-radio"
END SUB
