' FILE: Concurrency/GpmDeadlockSendNoReceive.bas
' =================================================================================================
'       Watchdog deadlock GPM: SEND senza alcuna RECEIVE corrispondente
' =================================================================================================
'
' Un canale bufferizzato (capacita' predefinita) si riempie e i SEND successivi restano bloccati
' per sempre: nessuna goroutine puo' fare progressi (nessun task eseguibile, nessun timer in
' attesa, nessuna I/O esterna pendente). Parita' con Go: il runtime NON deve restare appeso
' silenziosamente, ma rilevare lo stallo e terminare con diagnostica.
'
' Output atteso su stderr (non su stdout):
'   fatal error: all goroutines are asleep - deadlock!
' Il processo termina con codice di uscita diverso da zero entro pochi secondi (non un hang
' infinito). La riga "FINE PROGRAMMA" qui sotto non deve MAI comparire nell'output.

SUB Main()
  DIM Ris AS CHANNEL OF STRING
  DIM I AS INTEGER
  FOR I = 1 TO 999
    SEND "mai ricevuto" TO Ris
  NEXT I
  PRINT "FINE PROGRAMMA"
END SUB
