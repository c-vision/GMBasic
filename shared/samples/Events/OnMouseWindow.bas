' FILE: samples/Events/OnMouseWindow.bas
' ON MOUSE / ON WINDOW — registrazione eventi interattivi
' Demostra la registrazione di handler per eventi mouse e finestra.
' Nota: in esecuzione headless, gli eventi non vengono generati;
' le registrazioni sono conservate per quando un loop eventi sara' attivo.
SUB Main()
  ' ON MOUSE — registra handler per eventi del mouse
  PRINT "Registro ON MOUSE..."
  ON MOUSE Click GOSUB OnClick
  ON MOUSE Move GOSUB OnMove
  ON MOUSE Click CALL OnClickProc
  ON MOUSE Click STOP
  
  ' ON WINDOW — registra handler per eventi della finestra
  PRINT "Registro ON WINDOW..."
  ON WINDOW Resize GOSUB OnResize
  ON WINDOW Close GOSUB OnClose
  ON WINDOW Resize CALL OnResizeProc
  ON WINDOW Close STOP
  
  PRINT "Event handlers registrati."
END SUB

SUB OnClick
  PRINT "Mouse click"
END SUB

SUB OnMove
  PRINT "Mouse move"
END SUB

SUB OnClickProc
  PRINT "Click proc"
END SUB

SUB OnResize
  PRINT "Window resize"
END SUB

SUB OnClose
  PRINT "Window close"
END SUB

SUB OnResizeProc
  PRINT "Resize proc"
END SUB