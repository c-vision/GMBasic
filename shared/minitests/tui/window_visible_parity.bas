' Minitest WINDOW SHOW / WINDOW HIDE - parita' 4 backend (forma globale, visibilita' screen).
' Headless: no-op osservabile; verifica riconoscimento + emissione + stdout identico.
' EXPECT: window-visible-ok|DONE
IMPORT WINDOWS
SUB Main()
  WINDOW SHOW
  WINDOW HIDE
  PRINT "window-visible-ok"
  PRINT "DONE"
END SUB
