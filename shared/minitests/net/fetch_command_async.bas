' EXPECT: done= 1  err-nonempty=-1
' FETCH url INTO Callback — forma COMANDO asincrona: la callback (response$, error$) gira come task GPM quando la richiesta termina.
' Endpoint locale non in ascolto: la richiesta fallisce subito, la callback riceve error$ non vuoto (output indipendente dalla rete).
IMPORT HTTP
IMPORT CONCURRENCY
DIM gDone AS INTEGER
DIM gErr AS INTEGER

SUB OnFetch(resp$, err$)
  gErr = LEN(err$) > 0
  gDone = 1
END SUB

ASYNC SUB Starter(wg AS WAITGROUP)
  DIM i AS INTEGER
  FETCH "http://127.0.0.1:1/nope" INTO OnFetch()
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
  PRINT "done="; gDone; " err-nonempty="; gErr
END SUB
