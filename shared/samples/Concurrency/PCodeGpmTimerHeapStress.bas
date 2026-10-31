' Stress del timer heap PCodeVM: molte goroutine con deadline ripetute e fuori ordine.

IMPORT CONCURRENCY

SUB Sleeper(WorkerNo AS INTEGER, Results AS CHANNEL OF INTEGER)
  WAIT (WorkerNo MOD 7) + 1
  SEND WorkerNo TO Results
END SUB

SUB Main()
  DIM Results AS CHANNEL OF INTEGER
  DIM Workers AS WAITGROUP
  DIM Value AS INTEGER
  DIM Total AS LONG = 0

  FOR TaskNo = 1 TO 64
    GO SUB Sleeper(TaskNo, Results) IN Workers
  NEXT TaskNo

  FOR N = 1 TO 64
    RECEIVE Value FROM Results
    Total = Total + Value
  NEXT N

  WAIT Workers
  IF Total = 2080 THEN
    PRINT "PCODE-GPM-TIMER-HEAP-DONE"
  ELSE
    PRINT "FAIL:TIMER-TOTAL="; Total
  END IF
END SUB
