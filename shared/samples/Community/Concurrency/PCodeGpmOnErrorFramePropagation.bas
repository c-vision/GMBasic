' FILE: Concurrency/PCodeGpmOnErrorFramePropagation.bas
' Propagazione ON ERROR attraverso frame procedurali annidate, su due goroutine indipendenti.

IMPORT CONCURRENCY

SUB DeepFailure(Tag AS STRING)
  ERROR 901, "deep-" + Tag
END SUB

SUB MiddleCall(Tag AS STRING)
  DeepFailure(Tag)
  PRINT "FAIL:MIDDLE-CONTINUED:"; Tag
END SUB

SUB CallerWorker(Tag AS STRING)
  ON ERROR GOTO CallerHandler
  MiddleCall(Tag)
  PRINT "CALLER-RESUMED:"; Tag
  ON ERROR GOTO 0
  END SUB

CallerHandler:
  PRINT "CALLER-HANDLER"
  RESUME NEXT

SUB Main()
  DIM Workers AS WAITGROUP

  GO SUB CallerWorker("A") IN Workers
  GO SUB CallerWorker("B") IN Workers
  WAIT Workers

  PRINT "PCODE-GPM-ON-ERROR-FRAME-DONE"
END SUB
