' Minitest TUI DASHBOARD — parità 4 backend (statement widget TUI).
' EXPECT: OK-dashboard
IMPORT CONTROLS
SUB Main()
  DIM v AS INTEGER
  DASHBOARD 5, 2, 30, 6, ["CPU", "42", "100", "%"] INTO v
  SENDKEY CHR(27)
  PRINT "OK-dashboard"
END SUB
