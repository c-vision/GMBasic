' MiniTest: DOEVENTS (equivalente VB6) = un frame eventi NON bloccante (WINDOW PUMP 0).
' In TEXT/headless è no-op; in GRAPHICS drena input/presenta senza attendere.
' EXPECT: doevents-ok
SCREEN 12
PSET (10, 10), 7
DOEVENTS
SCREEN 0
PRINT "doevents-ok"
