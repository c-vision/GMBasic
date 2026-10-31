' FILE: Concurrency/PCodeGpmExtremeUnwind.bas
' Stress integrato per PCodeVM: preemption, frame-chain, TRY/CATCH, GOTO, canali e WAITGROUP.

IMPORT CONCURRENCY

SUB ThrowAtDepth3()
  THROW 703
END SUB

SUB ThrowAtDepth2()
  ThrowAtDepth3()
  PRINT "FAIL:DEPTH2-CONTINUED"
END SUB

SUB ThrowAtDepth1()
  ThrowAtDepth2()
  PRINT "FAIL:DEPTH1-CONTINUED"
END SUB

SUB ExceptionWorker(Results AS CHANNEL OF STRING)
  TRY
    ThrowAtDepth1()
    PRINT "FAIL:THROW-NOT-CAUGHT"
  CATCH Ex AS ERR
    SEND "EXCEPTION-CAUGHT" TO Results
  END TRY
END SUB

SUB GotoWorker(Results AS CHANNEL OF STRING)
  TRY
    GOTO OutsideProtectedRegion
  CATCH StaleEx AS ERR
    PRINT "FAIL:STALE-CATCH"
  END TRY

OutsideProtectedRegion:
  TRY
    THROW 704
  CATCH ActiveEx AS ERR
    SEND "GOTO-UNWIND-OK" TO Results
  END TRY
END SUB

SUB CpuWorker(Results AS CHANNEL OF STRING)
  DIM Acc AS DOUBLE = 0
  FOR I = 1 TO 100000
    Acc = Acc + I
  NEXT I
  IF Acc = 5000050000 THEN
    SEND "CPU-RESUME-OK" TO Results
  ELSE
    SEND "FAIL:CPU-RESULT" TO Results
  END IF
END SUB

SUB Main()
  DIM Results AS CHANNEL OF STRING
  DIM Workers AS WAITGROUP
  DIM Message AS STRING

  GO SUB CpuWorker(Results) IN Workers
  GO SUB ExceptionWorker(Results) IN Workers
  GO SUB GotoWorker(Results) IN Workers

  FOR N = 1 TO 3
    RECEIVE Message FROM Results
    PRINT Message
  NEXT N

  WAIT Workers
  PRINT "PCODE-GPM-EXTREME-DONE"
END SUB
