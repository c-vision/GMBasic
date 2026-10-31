' UserControlPropsDemo.bas — proprietà del controllo (§11.1) su una classe INHERITS Control:
' lettura `istanza.X/Y/Width/Height/Visible/...` e scrittura `istanza.X = v`.
IMPORT CONTROLS

CLASS Panel INHERITS Control
  SUB Paint(g AS ANY)
    CONTROL DRAW g, 0
  END SUB
END CLASS

SUB Main()
  DIM p AS Panel = NEW Panel()
  CONTROL ADD p AT 2, 2 SIZE 40, 12

  ' lettura proprietà (geometria/stato)
  PRINT "pos="; p.X; ","; p.Y; " size="; p.Width; "x"; p.Height

  ' scrittura proprietà → CONTROL SET
  p.X = 5
  p.Y = 6
  p.Width = 50
  p.Visible = 1

  PRINT "usercontrol-props-demo-ok"
  CONTROL REMOVE p
END SUB
