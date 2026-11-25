' MiniTest: un nome di CONTROLLO o di COMANDO seguito da `=` è una VARIABILE.
' REGRESSIONE C99Native (2026-11-25): il frontend C instrada le keyword di controllo a parser
' dedicati (WINDOW→gmb_p_parse_window, PANEL→gmb_p_parse_panel, TAB/GRID/CHART/CANVAS→
' gmb_p_parse_control) che consumavano `= …` come ARGOMENTI del controllo:
'   DIM panel AS LONG : panel = 4   →  gmb_form_add_panel((int)(=), (int)(4), ...)
' e il resto della riga spariva, mentre IL/PCode/VM-C lo trattano come assegnazione.
' Ora il dispatch delle istruzioni riconosce `KEYWORD = expr` (keyword con kind >= Define
' seguita da `=`/`:=`) e produce un ASSIGN, PRIMA dei rami dedicati.
' EXPECT: tab= 42|grid= 7|panel= 4|window= 1|sum= 54
SUB Main()
  DIM tab AS LONG
  DIM grid AS LONG
  DIM panel AS LONG
  DIM window AS LONG
  tab = 42
  grid = 7
  panel = 4
  window = 1
  PRINT "tab="; tab
  PRINT "grid="; grid
  PRINT "panel="; panel
  PRINT "window="; window
  PRINT "sum="; tab + grid + panel + window
END SUB
