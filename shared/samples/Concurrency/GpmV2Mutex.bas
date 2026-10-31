' FILE: Concurrency/GpmV2Mutex.bas
' =================================================================================================
'       G3 (parità Go sync.Mutex): MUTEX LOCK/UNLOCK sotto la GPM v2
' =================================================================================================
'
' Due goroutine competono per il mutex: la sezione critica è serializzata (LOCK parcheggia se
' il mutex è detenuto, UNLOCK fa handoff al waiter svegliato). Parità Go mu.Lock()/mu.Unlock().
'
' Output atteso (le tre sezioni critiche, ordine dei worker non deterministico):
'   CRIT 1 1 / CRIT 1 2 / CRIT 2 1 / MAIN-DONE

SUB Worker(MU AS MUTEX, WG AS WAITGROUP, N AS LONG, ID AS LONG)
  DIM I AS LONG
  MUTEX LOCK MU
  FOR I = 1 TO N
    SLEEP 2
    PRINT "CRIT "; ID; " "; I
  NEXT I
  MUTEX UNLOCK MU
  DONE WG
END SUB

SUB Main()
  DIM MU AS MUTEX
  DIM WG AS WAITGROUP
  GO SUB Worker(MU, WG, 2, 1) IN WG
  GO SUB Worker(MU, WG, 1, 2) IN WG
  WAIT WG
  PRINT "MAIN-DONE"
END SUB
