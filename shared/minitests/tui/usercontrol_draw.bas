' MiniTest UserControl: Paint usa CONTROL DRAW (primitiva GraphicsContext).
' EXPECT: OK-usercontrol-draw
IMPORT CONTROLS

CLASS Sea
  SUB Paint(g AS ANY)
    CONTROL DRAW g, 2, 0, 0, 50, 50      ' line
    CONTROL DRAW g, 7, 5, 5, 0, 0, "hi"  ' text
  END SUB
END CLASS

SUB Main()
  DIM c AS Sea = NEW Sea()
  CONTROL ADD c AT 10, 10 SIZE 100, 40
  PRINT "OK-usercontrol-draw"
END SUB
