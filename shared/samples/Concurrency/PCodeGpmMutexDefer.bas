' PCodeVM: MUTEX + DEFER (parità Go sync.Mutex e defer).
' Due goroutine incrementano un contatore condiviso proteggendolo con LOCK/UNLOCK.
' DEFER MUTEX UNLOCK mu assicura il rilascio a fine procedura (parità Go defer mu.Unlock()).
' Senza il mutex il totale sarebbe non deterministico; con il lock deve essere esatto.

IMPORT CONCURRENCY

DIM Counter AS INTEGER = 0

SUB Incrementer(mu AS MUTEX, wg AS WAITGROUP)
  DEFER MUTEX UNLOCK mu
  DIM v AS INTEGER
  MUTEX LOCK mu
  ' Sezione critica: legge il contatore globale, incrementa, riscrive.
  ' L'UNLOCK è lasciato al DEFER (parità Go defer mu.Unlock()).
  v = Counter
  v = v + 1
  Counter = v
END SUB

SUB Main()
  DIM mu AS MUTEX
  DIM wg AS WAITGROUP
  DIM i AS INTEGER
  Counter = 0
  FOR i = 1 TO 10
    GO SUB Incrementer(mu, wg) IN wg
  NEXT i
  WAIT wg
  IF Counter = 10 THEN
    PRINT "PCODE-GPM-MUTEX-DEFER-OK"
  ELSE
    PRINT "FAIL:COUNTER="; Counter
  END IF
END SUB
