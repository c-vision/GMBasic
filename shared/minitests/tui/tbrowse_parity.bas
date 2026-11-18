' Minitest TUI TBROWSE — parità 4 backend (statement widget TUI).
' EXPECT: OK-tbrowse
IMPORT CONTROLS
SUB Main()
  DIM cl(2) AS STRING
  DIM dt(2, 2) AS STRING
  cl(0) = "A"
  dt(0, 0) = "1"
  DIM v AS INTEGER
  TBROWSE 34, 2, 70, 15, cl, dt INTO v
  PRINT "OK-tbrowse"
END SUB
