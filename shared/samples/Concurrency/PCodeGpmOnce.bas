' PCodeVM: ONCE (parità Go sync.Once) — la procedura Init viene eseguita una sola volta
' anche se 5 goroutine chiamano ONCE RUN in concorrenza. Il flag Counter deve essere 1
' (eseguita dal primo chiamante); gli altri attendono e proseguono senza ri-eseguirla.
IMPORT CONCURRENCY
DIM Counter AS LONG = 0

SUB Init(o AS ONCE, wg AS WAITGROUP)
  ONCE RUN o, DoInit
END SUB

SUB DoInit()
  Counter = Counter + 1
  SLEEP 1
END SUB

SUB Main()
  DIM o AS ONCE
  DIM wg AS WAITGROUP
  DIM i AS INTEGER
  FOR i = 1 TO 5
    GO SUB Init(o, wg) IN wg
  NEXT i
  WAIT wg
  IF Counter = 1 THEN
    PRINT "PCODE-GPM-ONCE-OK"
  ELSE
    PRINT "FAIL:COUNTER="; Counter
  END IF
END SUB
