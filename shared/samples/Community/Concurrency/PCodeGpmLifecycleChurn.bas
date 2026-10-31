' PCodeVM: stress lifecycle/reclaim — 500 goroutine spawnate in sequenza da Main,
' ognuna fa un round-trip su canale e termina (reclaim incrementale PCB). Con
' GOMAXPROCS=1 tutte devono completare e la somma deve essere esatta; se il reclaim
' perdessero task, un RECEIVE resterebbe bloccato e il runner scaderebbe (bounded).

IMPORT CONCURRENCY

SUB Worker(Id AS INTEGER, Jobs AS CHANNEL OF INTEGER, Results AS CHANNEL OF INTEGER)
  DIM v AS INTEGER
  RECEIVE v FROM Jobs
  SEND v * 2 TO Results
END SUB

SUB Main()
  DIM Jobs AS CHANNEL OF INTEGER
  DIM Results AS CHANNEL OF INTEGER
  DIM wg AS WAITGROUP
  DIM Total AS LONG = 0
  DIM i AS INTEGER

  FOR i = 1 TO 500
    GO SUB Worker(i, Jobs, Results) IN wg
  NEXT i

  FOR i = 1 TO 500
    SEND i TO Jobs
  NEXT i

  FOR i = 1 TO 500
    DIM v AS INTEGER
    RECEIVE v FROM Results
    Total = Total + v
  NEXT i

  WAIT wg
  IF Total = 250500 THEN
    PRINT "PCODE-GPM-LIFECYCLE-OK"
  ELSE
    PRINT "FAIL:LIFECYCLE-TOTAL="; Total
  END IF
END SUB
