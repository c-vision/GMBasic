' PCodeVM: un WAITKEY parcheggiato (PCodeWaitKind.Input) non deve immobilizzare
' l'unico worker: la goroutine Ticker deve progredire mentre InputTask attende.
' Il runner cancella il run dopo un bound: WAITKEY riprende con cancellazione
' (abort finestra) e il task unwinda — l'input park è una registration cancellabile.

IMPORT CONCURRENCY

SUB InputTask(wg AS WAITGROUP)
  WAITKEY
  PRINT "WAITKEY-RESUMED"
  DONE wg
END SUB

SUB Ticker(wg AS WAITGROUP)
  SLEEP 50
  PRINT "INPUT-TICK"
  DONE wg
END SUB

SUB Main()
  DIM wg AS WAITGROUP
  GO SUB InputTask(wg) IN wg
  GO SUB Ticker(wg) IN wg
  WAIT wg
  PRINT "INPUT-DONE"
END SUB
