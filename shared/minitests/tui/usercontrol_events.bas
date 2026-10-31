' Minitest CONTROLS metodi-evento UserControl — un metodo della classe col nome di un evento
' (Click/Change/GotFocus/…) è cablato all'evento del controllo ed eseguito come TASK GPM (SELF).
' §11.7-3.
' NB: la gamba IL/PCode headless usa NullGraphicsService → il UserControl non si registra
'     (verifica comportamentale sulle gambe C: VM-C/C99Native).
' EXPECT: ucev-ok
IMPORT CONTROLS

CLASS Box INHERITS Control
  SUB Change()
  END SUB
  SUB Paint(g AS ANY)
  END SUB
END CLASS

SUB Main()
  DIM b AS Box = NEW Box()
  DIM h AS LONG
  CONTROL ADD b AT 1, 1 SIZE 20, 8 INTO h
  FIRE EVENT h, "CHANGE"
  FIRE EVENT h, "CHANGE"
  CONTROL REMOVE b
  PRINT "ucev-ok"
END SUB
