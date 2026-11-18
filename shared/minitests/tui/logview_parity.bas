' Minitest TUI LOGVIEW — parità 4 backend (statement widget TUI).
' EXPECT: OK-logview
IMPORT CONTROLS
SUB Main()
  DIM v AS INTEGER
  LOGVIEW 7, 5, 30, 6, ["avvio", "ok", "fine"] INTO v
  PRINT "OK-logview"
END SUB
