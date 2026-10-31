' Stress GO SELECT: tre producer rendono pronti case diversi mentre il consumer
' si parcheggia ripetutamente. Ogni scelta deve lasciare zero waiter perdenti.

IMPORT CONCURRENCY

SUB Produce(C AS CHANNEL OF INTEGER, Value AS INTEGER)
  FOR I = 1 TO 50
    SEND Value TO C
  NEXT I
END SUB

SUB Main()
  DIM C1 AS CHANNEL OF INTEGER
  DIM C2 AS CHANNEL OF INTEGER
  DIM C3 AS CHANNEL OF INTEGER
  DIM Workers AS WAITGROUP
  DIM Total AS LONG = 0

  GO SUB Produce(C1, 1) IN Workers
  GO SUB Produce(C2, 10) IN Workers
  GO SUB Produce(C3, 100) IN Workers

  FOR N = 1 TO 150
    GO SELECT
      CASE RECEIVE V1 FROM C1:
        Total = Total + V1
      CASE RECEIVE V2 FROM C2:
        Total = Total + V2
      CASE RECEIVE V3 FROM C3:
        Total = Total + V3
    END SELECT
  NEXT N

  WAIT Workers
  IF Total = 5550 THEN
    PRINT "PCODE-GPM-SELECT-CLEANUP-DONE"
  ELSE
    PRINT "FAIL:SELECT-TOTAL="; Total
  END IF
END SUB
