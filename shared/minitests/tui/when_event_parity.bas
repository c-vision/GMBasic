' Minitest evento WHEN + FIRE EVENT (dispatch sincrono su tutte e 4 le gambe) — parità.
' EXPECT: WHEN-FIRED|OK-when-event
IMPORT CONTROLS
SUB Main()
  DIM h AS INTEGER
  LABEL 5, 2, 20, "x" ID h ON WHEN OnWhen
  FIRE EVENT h, "WHEN"
  PRINT "OK-when-event"
END SUB
SUB OnWhen()
  PRINT "WHEN-FIRED"
END SUB
