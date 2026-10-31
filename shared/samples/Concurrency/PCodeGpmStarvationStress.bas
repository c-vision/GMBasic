' Stress starvation PCodeVM: 32 goroutine concorrenti su un solo P (GOMAXPROCS=1)
' devono tutte completare e consegnare il risultato esatto — nessuna affamata.
' Ogni worker alterna lavoro CPU a SLEEP (park) e consegna la somma attesa via canale.
' Se una goroutine fosse monopolizzata/affamata, Main resterebbe bloccato su RECEIVE
' e il runner scaderebbe (bounded): il completamento entro il bound è l'asserzione.

IMPORT CONCURRENCY

SUB Worker(Id AS INTEGER, Results AS CHANNEL OF INTEGER)
  DIM X AS INTEGER = 0
  DIM I AS INTEGER
  FOR I = 1 TO 300
    X = X + I * (Id + 1)
    IF I MOD 50 = 0 THEN SLEEP 1
  NEXT I
  SEND X TO Results
END SUB

SUB Main()
  DIM Results AS CHANNEL OF INTEGER
  DIM Workers AS WAITGROUP
  DIM Value AS INTEGER
  DIM Total AS LONG = 0
  DIM Expected AS LONG = 0

  FOR TaskNo = 1 TO 32
    GO SUB Worker(TaskNo, Results) IN Workers
    Expected = Expected + (TaskNo + 1) * 300 * 301 / 2
  NEXT TaskNo

  FOR N = 1 TO 32
    RECEIVE Value FROM Results
    Total = Total + Value
  NEXT N

  WAIT Workers
  IF Total = Expected THEN
    PRINT "PCODE-GPM-STARVATION-OK"
  ELSE
    PRINT "FAIL:STARVATION-TOTAL="; Total
  END IF
END SUB
