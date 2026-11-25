' MiniTest: un nome di CONTROLLO seguito da `=` è una VARIABILE, non un controllo.
' REGRESSIONE C99Native (2026-11-25): il dispatcher delle istruzioni del frontend C instrada
' ogni keyword di controllo (TAB, GRID, PANEL, ...) a `gmb_p_parse_control`, quindi
' `DIM tab AS LONG : tab = 42` diventava `CONTROL 'tab'` e l'emissione falliva con
' "CONTROL 'tab' non supportato dall'emitter nativo", mentre IL/PCode/VM-C lo trattano come
' assegnazione. Ora in `gmb_p_parse_control` un nome seguito da `=`/`:=` produce un ASSIGN.
' EXPECT: tab= 42|grid= 7|sum= 49
SUB Main()
  DIM tab AS LONG
  DIM grid AS LONG
  tab = 42
  grid = 7
  PRINT "tab="; tab
  PRINT "grid="; grid
  PRINT "sum="; tab + grid
END SUB
