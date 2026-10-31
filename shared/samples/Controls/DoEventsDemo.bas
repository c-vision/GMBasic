' DoEventsDemo — DOEVENTS (VB6): pompa un frame di eventi e ritorna (non bloccante).
' Utile nei loop di disegno/calcolo per mantenere la UI reattiva.
SCREEN 12, 320, 200, 32
FOR i = 1 TO 30
  PSET (i * 10, 100), 15
  LINE (i * 10, 60)-(i * 10, 140), 7
  DOEVENTS
NEXT i
SCREEN 0
PRINT "DoEventsDemo ok"
