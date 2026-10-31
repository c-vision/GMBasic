' MiniTest: WAITKEY nudo (gruppo Console, parità 4 backend).
' WAITKEY attende un tasto; con SENDKEY pre-iniettato ritorna subito.
' Reale sulle 4 gambe. L'oracolo è il marker finale (prova uscita + no-hang).
' EXPECT: wait=ok
IMPORT CONSOLE

SUB Main()
  SENDKEY "Q"
  WAITKEY
  PRINT "wait=ok"
END SUB
