' Minitest TUI TREEVIEW — parità 4 backend (statement widget TUI).
' EXPECT: OK-treeview
IMPORT CONTROLS
SUB Main()
  DIM it(2) AS STRING
  it(0) = "root"
  DIM v AS INTEGER
  TREEVIEW 30, 3, 30, 10, it INTO v
  PRINT "OK-treeview"
END SUB
