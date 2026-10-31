' PCodeVM: stress estremo misto multi-P — 400 goroutine che combinano RECEIVE da canale,
' lavoro CPU (sum loop), SLEEP (park timer) e SEND a canale, con GOMAXPROCS fino a 32
' (oversubscription: più worker che core). Il totale è deterministico (il risultato di ogni
' worker dipende SOLO dal valore del job ricevuto, non dall'ordine di scheduling): una race
' dello scheduler (wake perso, doppia esecuzione, deadlock) lo romperebbe o appenderebbe.
' Totale atteso = 5050 * sum(1..400) = 5050 * 80200 = 405010000.

IMPORT CONCURRENCY

SUB Worker(Id AS INTEGER, Jobs AS CHANNEL OF INTEGER, Results AS CHANNEL OF INTEGER)
  DIM v AS INTEGER
  RECEIVE v FROM Jobs
  DIM x AS LONG = 0
  DIM i AS INTEGER
  FOR i = 1 TO 100
    x = x + i * v
    IF i MOD 25 = 0 THEN SLEEP 1
  NEXT i
  SEND x TO Results
END SUB

SUB Main()
  DIM n AS LONG = 400
  DIM Jobs AS CHANNEL OF INTEGER
  DIM Results AS CHANNEL OF INTEGER
  DIM wg AS WAITGROUP
  DIM Total AS LONG = 0
  DIM i AS INTEGER
  FOR i = 1 TO n
    GO SUB Worker(i, Jobs, Results) IN wg
  NEXT i
  FOR i = 1 TO n
    SEND i TO Jobs
  NEXT i
  FOR i = 1 TO n
    DIM v AS INTEGER
    RECEIVE v FROM Results
    Total = Total + v
  NEXT i
  WAIT wg
  IF Total = 405010000 THEN
    PRINT "PCODE-GPM-STRESS-MIX-OK"
  ELSE
    PRINT "FAIL:STRESS-MIX-TOTAL="; Total
  END IF
END SUB
