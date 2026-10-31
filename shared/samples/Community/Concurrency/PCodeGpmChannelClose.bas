' PCodeVM: CHANNEL CLOSE + RECEIVE ... OK flag (parità Go close(ch) e v, ok := <-ch).
' Il Main chiude il canale jobs DOPO l'invio; i worker escono dal loop RECEIVE
' quando il flag ok diventa %CHCLOSED (0) — il pattern di shutdown Go-idiomatico
' per un worker pool. Totale deterministico = sum(1..10)*2 = 110.
' NOTA capacity: il default dei canali è 0 (unbuffered, parità Go make(chan T)); questo
' pattern produttore→worker→risultati a due canali richiede un buffer esplicito per non
' deadlockare (produttore bloccato mentre i worker bloccati sul rinvia), quindi jobs e
' results dichiarano la capacità esplicita (10) come make(chan int, 10).

IMPORT CONCURRENCY

SUB Worker(id AS INTEGER, jobs AS CHANNEL OF INTEGER, results AS CHANNEL OF INTEGER, wg AS WAITGROUP)
  DIM job AS INTEGER
  DIM ok AS BIT
  DO
    RECEIVE job FROM jobs OK ok
    IF ok = %CHCLOSED THEN EXIT DO
    SEND job * 2 TO results
  LOOP
  DONE wg
END SUB

SUB Main()
  DIM jobs AS CHANNEL OF INTEGER (10)
  DIM results AS CHANNEL OF INTEGER (10)
  DIM wg AS WAITGROUP
  DIM i AS INTEGER
  DIM v AS INTEGER
  DIM Total AS LONG = 0

  GO SUB Worker(1, jobs, results, wg) IN wg
  GO SUB Worker(2, jobs, results, wg) IN wg
  GO SUB Worker(3, jobs, results, wg) IN wg

  FOR i = 1 TO 10
    SEND i TO jobs
  NEXT i
  CHANNEL CLOSE jobs

  FOR i = 1 TO 10
    RECEIVE v FROM results
    Total = Total + v
  NEXT i

  WAIT wg
  ' sum(1..10) * 2 = 110
  IF Total = 110 THEN
    PRINT "PCODE-GPM-CHANNEL-CLOSE-OK"
  ELSE
    PRINT "FAIL:CLOSE-TOTAL="; Total
  END IF
END SUB
