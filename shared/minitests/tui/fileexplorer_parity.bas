' Minitest TUI FILEEXPLORER — parità 4 backend (statement widget TUI).
IMPORT CONTROLS
SUB Main()
  DIM v AS INTEGER
  FILEEXPLORER 5, 2, 30, 6, "." INTO v
  SENDKEY CHR(27)
  PRINT "OK-fileexplorer"
END SUB
