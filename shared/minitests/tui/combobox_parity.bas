' Minitest TUI COMBOBOX — parità 4 backend (statement widget TUI).
' EXPECT: OK-combobox
IMPORT CONTROLS
SUB Main()
  DIM v AS INTEGER
  COMBOBOX 19, 5, 20, ["IT", "FR", "DE"] INTO v
  PRINT "OK-combobox"
END SUB
