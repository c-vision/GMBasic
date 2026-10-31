' EXPECT: on-mouse-reg
' ON MOUSE — registrazione handler eventi mouse (parità 4 backend)
SUB Main()
  ON MOUSE Click GOSUB mouseHandler
  ON MOUSE Click CALL myProc()
  ON MOUSE Click STOP
  PRINT "on-mouse-reg"
END SUB
SUB mouseHandler
END SUB
SUB myProc
END SUB