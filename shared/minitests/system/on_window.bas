' TAG: interactive
' EXPECT: on-window-reg
' ON WINDOW — registrazione handler eventi finestra (parità 4 backend)
SUB Main()
  ON WINDOW Resize GOSUB windowHandler
  ON WINDOW Resize CALL myProc()
  ON WINDOW Close STOP
  PRINT "on-window-reg"
END SUB
SUB windowHandler
END SUB
SUB myProc
END SUB