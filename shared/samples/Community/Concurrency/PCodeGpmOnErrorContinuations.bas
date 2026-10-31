' FILE: Concurrency/PCodeGpmOnErrorContinuations.bas
' Stress PCodeVM per continuazioni ON ERROR isolate per goroutine.

IMPORT CONCURRENCY

DIM GosubSteps AS INTEGER = 0

SUB GosubWorker()
  ON ERROR GOSUB GosubHandler
  ERROR 801, "first"
  GosubSteps = GosubSteps + 1
  PRINT "ON-ERROR-GOSUB-AFTER-FIRST"
  ERROR 802, "second"
  GosubSteps = GosubSteps + 1
  PRINT "ON-ERROR-GOSUB-AFTER-SECOND"
  ON ERROR GOTO 0
  IF GosubSteps = 4 THEN
    PRINT "ON-ERROR-GOSUB-STICKY-OK"
  ELSE
    PRINT "FAIL:GOSUB-CONTINUATION:"; GosubSteps
  END IF
  END SUB

GosubHandler:
  GosubSteps = GosubSteps + 1
  PRINT "ON-ERROR-GOSUB-HANDLER"
  RETURN

SUB ResumeNextWorker()
  DIM Steps AS INTEGER = 0

  ON ERROR GOTO ResumeHandler
  ERROR 803, "resume-next"
  Steps = Steps + 1
  GOTO ResumeDone

ResumeHandler:
  Steps = Steps + 1
  RESUME NEXT

ResumeDone:
  ON ERROR GOTO 0
  IF Steps = 2 THEN
    PRINT "ON-ERROR-RESUME-NEXT-OK"
  ELSE
    PRINT "FAIL:RESUME-NEXT"
  END IF
END SUB

SUB Main()
  DIM Workers AS WAITGROUP

  GO SUB GosubWorker() IN Workers
  GO SUB ResumeNextWorker() IN Workers

  WAIT Workers
  PRINT "PCODE-GPM-ON-ERROR-DONE"
END SUB
