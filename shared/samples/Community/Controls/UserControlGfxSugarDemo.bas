' UserControlGfxSugarDemo.bas — zucchero GraphicsContext nel metodo Paint.
' Sul `g` di Paint (ANY): g.Clear/SetColor/Line/Rect/FillRect/Circle/FillCircle/Plot/Text
' invece di `CONTROL DRAW g, op, …`.
IMPORT CONTROLS

CLASS Panel INHERITS Control
  SUB Paint(g AS ANY)
    g.Clear()
    g.SetColor(8421504)              ' grigio
    g.Rect(0, 0, 38, 10)             ' bordo
    g.SetColor(65280)                ' verde
    g.Line(0, 0, 38, 10)             ' diagonale
    g.Circle(10, 5, 3)
    g.Plot(2, 2)
    g.Text("pannello", 3, 2)
  END SUB
END CLASS

SUB Main()
  DIM p AS Panel = NEW Panel()
  CONTROL ADD p AT 2, 2 SIZE 40, 12
  CONTROL REMOVE p
  PRINT "usercontrol-gcsugar-demo-ok"
END SUB
