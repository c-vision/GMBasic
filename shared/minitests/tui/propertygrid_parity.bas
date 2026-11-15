' Minitest TUI PROPERTYGRID — parità 4 backend (statement widget TUI).
' EXPECT: OK-propertygrid
IMPORT CONTROLS
SUB Main()
  DIM v AS INTEGER
  PROPERTYGRID 7, 5, 30, 5, ["Nome", "Valore", "A", "1"] INTO v
  PRINT "OK-propertygrid"
END SUB
