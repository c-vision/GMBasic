' Minitest CONTROLS proprietà controllo — `istanza.X/Y/Width/Height/...` (lettura) e
' `istanza.X = v` (scrittura) su una classe INHERITS Control → CONTROL GET/SET. §11.1.
' EXPECT: ucprop-ok
IMPORT CONTROLS

CLASS Box INHERITS Control
  SUB Paint(g AS ANY)
  END SUB
END CLASS

SUB Main()
  DIM b AS Box = NEW Box()
  CONTROL ADD b AT 3, 4 SIZE 20, 8
  b.X = 10
  b.Visible = 1
  b.ForeColor = "#FF8000"
  b.BackColor = "#003060"
  IF b.X = 10 AND b.Y = 4 AND b.Width = 20 AND b.Height = 8 AND b.Visible = 1 AND b.ForeColor = "#FF8000" AND b.BackColor = "#003060" THEN
    PRINT "ucprop-ok"
  ELSE
    PRINT "ucprop-no " + STR(b.X) + "," + STR(b.Y) + "," + STR(b.Width) + "," + STR(b.Height) + " fg=" + b.ForeColor + " bg=" + b.BackColor
  END IF
  CONTROL REMOVE b
END SUB
