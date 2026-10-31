' UserControlEventsDemo.bas — eventi UserControl (§11.7-3): un metodo della classe col nome di un
' evento è cablato all'evento del controllo ed eseguito come TASK GPM (SELF = istanza).
' Nota: verifica comportamentale sulle gambe C (grafica/VM); headless IL/PCode non registra.
IMPORT CONTROLS

DIM g_ticks AS INTEGER   ' stato osservabile aggiornato dall'handler

CLASS Panel INHERITS Control
  SUB Change()
    g_ticks = g_ticks + 1
  END SUB
  SUB GotFocus()
    g_ticks = g_ticks + 10
  END SUB
  SUB Paint(g AS ANY)
    CONTROL DRAW g, 0
  END SUB
END CLASS

SUB Main()
  DIM p AS Panel = NEW Panel()
  DIM h AS LONG
  CONTROL ADD p AT 2, 2 SIZE 40, 12 INTO h

  FIRE EVENT h, "CHANGE"      ' → Panel.Change (task GPM)
  FIRE EVENT h, "GOTFOCUS"    ' → Panel.GotFocus (task GPM)

  PRINT "usercontrol-events-demo-ok"; g_ticks
  CONTROL REMOVE p
END SUB
