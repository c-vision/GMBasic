' Minitest TUI BIGTEXT — parità 4 backend (statement widget TUI, display-only).
' EXPECT: OK-bigtext
IMPORT CONTROLS
SUB Main()
  BIGTEXT 5, 2, "ABC"
  PRINT "OK-bigtext"
END SUB
