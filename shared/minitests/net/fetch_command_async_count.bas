' EXPECT: calls= 1|done= 1
' FETCH url INTO Callback — la callback DEVE essere eseguita ESATTAMENTE una volta.
' Complementa fetch_command_async.bas, che verifica solo il flag finale: qui si conta quante
' volte la SUB viene invocata, cosi' si distingue "non parte" (0) da "parte due volte" (2) —
' difetti diversi, con la stessa osservazione sul flag.
' Endpoint locale non in ascolto: la richiesta fallisce subito, l'errore e' l'unica cosa che
' la callback puo' osservare (output indipendente dalla rete e dai tempi).
IMPORT HTTP
IMPORT CONCURRENCY

DIM gCalls AS INTEGER
DIM gDone AS INTEGER

SUB OnFetch(resp$, err$)
  gCalls = gCalls + 1
  gDone = 1
END SUB

ASYNC SUB Starter(wg AS WAITGROUP)
  FETCH "http://127.0.0.1:1/nope" INTO OnFetch()
  DIM i AS INTEGER
  FOR i = 1 TO 400
    IF gDone = 1 THEN EXIT FOR
    AWAIT CONCURRENCY.SLEEPASYNC(25)
  NEXT i
  DONE wg
END SUB

SUB Main()
  DIM wg AS WAITGROUP
  GO SUB Starter(wg) IN wg
  WAIT wg
  PRINT "calls="; gCalls
  PRINT "done="; gDone
END SUB