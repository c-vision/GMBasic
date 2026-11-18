' Minitest TUI FILEDIALOG — parità 4 backend (statement widget TUI).
' EXPECT: OK-filedialog
IMPORT CONTROLS
SUB Main()
  DIM p AS STRING
  FILEDIALOG "*.bas", "select" INTO p
  PRINT "OK-filedialog"
END SUB
