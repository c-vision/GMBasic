' SplitterDemo.bas — demo d'applicazione del controllo (Controls/Splitter.bas via IMPORT).
IMPORT CustomControls

SUB Main()
  DIM s AS Splitter = NEW Splitter()
  CONTROL ADD s AT 1, 1 SIZE 40, 10
  s.Configure(0, 40, 1, 10)          ' verticale: 40 largo, barra 1, altezza 10
  PRINT "pos0="; s.Pos()
  PRINT "paneA w="; s.PaneAW(); " h="; s.PaneAH()
  PRINT "paneB x="; s.PaneBX(); " w="; s.PaneBW()
  ' trascina la barra
  s.MousePress(20, 5)                 ' preme sulla barra
  PRINT "dragging="; s.Dragging()
  s.MouseDrag(30, 5)                  ' trascina a x=30
  s.MouseRelease()
  PRINT "pos1="; s.Pos(); " Bx="; s.PaneBX(); " Bw="; s.PaneBW()
  ' clamp ai minimi
  s.SetPos(100)
  PRINT "pos-clamp="; s.Pos()
  CONTROL REMOVE s
  PRINT "splitter-ok"
END SUB
