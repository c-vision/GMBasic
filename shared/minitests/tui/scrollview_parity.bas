' Minitest TUI SCROLLVIEW — parità 4 backend (statement widget TUI).
' EXPECT: OK-scrollview
IMPORT CONTROLS
SUB Main()
  DIM v AS INTEGER
  SCROLLVIEW 7, 5, 30, 5, ["riga1", "riga2", "riga3"] INTO v
  PRINT "OK-scrollview"
END SUB
