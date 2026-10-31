' Minitest TUI GRID — parità 4 backend (statement widget TUI).
IMPORT CONTROLS
SUB Main()
  DIM hd(3) AS STRING
  DIM dt(2, 2) AS STRING
  hd(0) = "A"
  hd(1) = "B"
  dt(0, 0) = "1"
  DIM v AS INTEGER
  GRID 22, 3, 60, 5, hd, dt INTO v
  PRINT "OK-grid"
END SUB
