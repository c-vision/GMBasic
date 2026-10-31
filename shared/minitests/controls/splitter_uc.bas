' Minitest Splitter UserControl (§12.4) — due pannelli + barra trascinabile, 4 backend.
' EXPECT: splitter-ok
IMPORT CONTROLS
IMPORT CustomControls

SUB Main()
  DIM s AS Splitter = NEW Splitter()
  CONTROL ADD s AT 1, 1 SIZE 40, 10
  s.Configure(0, 40, 1, 10)
  DIM p0 AS INTEGER
  p0 = s.Pos()
  DIM aw AS INTEGER
  aw = s.PaneAW()
  s.MousePress(19, 5)
  DIM dg AS INTEGER
  dg = s.Dragging()
  s.MouseDrag(30, 5)
  s.MouseRelease()
  DIM p1 AS INTEGER
  p1 = s.Pos()
  DIM bx AS INTEGER
  bx = s.PaneBX()
  s.SetPos(100)
  DIM pc AS INTEGER
  pc = s.Pos()
  CONTROL REMOVE s
  IF p0 = 19 AND aw = 19 AND dg = 1 AND p1 = 30 AND bx = 31 AND pc = 36 THEN
    PRINT "splitter-ok"
  ELSE
    PRINT "splitter-no "; p0; "|"; aw; "|"; dg; "|"; p1; "|"; bx; "|"; pc
  END IF
END SUB
