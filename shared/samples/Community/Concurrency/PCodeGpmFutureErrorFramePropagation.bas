' FILE: Concurrency/PCodeGpmFutureErrorFramePropagation.bas
' Future GO FUNCTION fallito, implicit-await e unwind verso ON ERROR del chiamante.

IMPORT CONCURRENCY

FUNCTION FailedCalculation() AS LONG
  ERROR 951, "future-failure"
  RETURN 0
END FUNCTION

SUB AwaitFailedFuture()
  DIM Value AS LONG
  GO FUNCTION FailedCalculation() INTO Value
  PRINT "FAIL:FUTURE-NOT-PROPAGATED:"; Value
END SUB

SUB FutureCaller()
  ON ERROR GOTO FutureHandler
  AwaitFailedFuture()
  PRINT "FUTURE-CALLER-RESUMED"
  ON ERROR GOTO 0
  END SUB

FutureHandler:
  PRINT "FUTURE-CALLER-HANDLER"
  RESUME NEXT

SUB Main()
  DIM Workers AS WAITGROUP
  GO SUB FutureCaller() IN Workers
  WAIT Workers
  PRINT "PCODE-GPM-FUTURE-FRAME-DONE"
END SUB
