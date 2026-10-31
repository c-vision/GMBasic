' PCodeVM: RWMUTEX (parità Go sync.RWMutex) — lettori condivisi (RLOCK/RUNLOCK) +
' scrittore esclusivo (LOCK/UNLOCK). Uno scrittore incrementa un contatore condiviso;
' i lettori lo leggono sotto RLOCK. Totale deterministico: Shared = 1.
IMPORT CONCURRENCY
DIM Shared AS LONG = 0

SUB Reader(rw AS RWMUTEX, wg AS WAITGROUP, id AS LONG)
  DIM v AS LONG
  MUTEX RLOCK rw
  v = Shared
  PRINT "READ"; id; "="; v
  MUTEX RUNLOCK rw
END SUB

SUB Writer(rw AS RWMUTEX, wg AS WAITGROUP)
  MUTEX LOCK rw
  Shared = Shared + 1
  MUTEX UNLOCK rw
END SUB

SUB Main()
  DIM rw AS RWMUTEX
  DIM wg AS WAITGROUP
  GO SUB Reader(rw, wg, 1) IN wg
  GO SUB Writer(rw, wg) IN wg
  GO SUB Reader(rw, wg, 2) IN wg
  WAIT wg
  IF Shared = 1 THEN
    PRINT "PCODE-GPM-RWMUTEX-OK"
  ELSE
    PRINT "FAIL:SHARED="; Shared
  END IF
END SUB
