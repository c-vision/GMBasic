' MiniTest GraphicsContext sugar — `g.Clear/SetColor/Line/Rect/FillRect/Circle/FillCircle/Text/Plot`.
' Su una variabile ANY (il `g` di Paint) → `CONTROL DRAW g, op, …`. §11.7-2.
' EXPECT: gcsugar-ok
IMPORT CONTROLS

CLASS Box INHERITS Control
  SUB Paint(g AS ANY)
    g.Clear()
    g.SetColor(65280)
    g.Line(0, 0, 20, 10)
    g.Rect(1, 1, 10, 5)
    g.FillRect(2, 2, 8, 3)
    g.Circle(5, 5, 3)
    g.FillCircle(6, 6, 2)
    g.Plot(1, 1)
    g.Text("hi", 2, 2)
  END SUB
END CLASS

SUB Main()
  DIM b AS Box = NEW Box()
  CONTROL ADD b AT 1, 1 SIZE 20, 8
  CONTROL REMOVE b
  PRINT "gcsugar-ok"
END SUB
