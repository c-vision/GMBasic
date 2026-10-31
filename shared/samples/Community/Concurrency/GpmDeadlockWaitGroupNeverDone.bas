' FILE: Concurrency/GpmDeadlockWaitGroupNeverDone.bas
' =================================================================================================
'       Watchdog deadlock GPM: WAIT su un WAITGROUP la cui goroutine non chiama mai DONE
' =================================================================================================
'
' La goroutine lanciata resta bloccata in RECEIVE su un canale su cui nessuno invia mai nulla,
' quindi non raggiunge mai la DONE WG. Il WAIT WG del chiamante resta percio' bloccato per
' sempre: nessun task eseguibile, nessun timer in attesa, nessuna I/O esterna pendente.
'
' Output atteso su stderr (non su stdout):
'   fatal error: all goroutines are asleep - deadlock!
' Il processo termina con codice di uscita diverso da zero entro pochi secondi (non un hang
' infinito). La riga "FINE PROGRAMMA" qui sotto non deve MAI comparire nell'output.

SUB Worker(InC AS CHANNEL OF STRING, WG AS WAITGROUP)
  DIM V AS STRING
  RECEIVE V FROM InC
  DONE WG
END SUB

SUB Main()
  DIM InC AS CHANNEL OF STRING
  DIM WG AS WAITGROUP
  GO SUB Worker(InC, WG) IN WG
  WAIT WG
  PRINT "FINE PROGRAMMA"
END SUB
