' Con GOMAXPROCS=1, SLEEP 0 deve cedere il worker prima di continuare.

IMPORT CONCURRENCY

SUB YieldingWorker(Results AS CHANNEL OF STRING)
  SLEEP 0
  SEND "A-AFTER-YIELD" TO Results
END SUB

SUB ReadyWorker(Results AS CHANNEL OF STRING)
  SEND "B-RAN" TO Results
END SUB

SUB Main()
  DIM Results AS CHANNEL OF STRING
  DIM Workers AS WAITGROUP
  DIM First AS STRING
  DIM Second AS STRING

  GO SUB YieldingWorker(Results) IN Workers
  GO SUB ReadyWorker(Results) IN Workers

  RECEIVE First FROM Results
  RECEIVE Second FROM Results
  WAIT Workers

  IF First = "B-RAN" AND Second = "A-AFTER-YIELD" THEN
    PRINT "PCODE-GPM-SLEEP-ZERO-YIELD-DONE"
  ELSE
    PRINT "FAIL:SLEEP-ZERO-ORDER="; First; ","; Second
  END IF
END SUB
