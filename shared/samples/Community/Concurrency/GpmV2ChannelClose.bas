' FILE: Concurrency/GpmV2ChannelClose.bas
' =================================================================================================
'       G3 (parità Go 1:1): CHANNEL CLOSE + RECEIVE con OK flag + CAPACITY nella GPM v2
' =================================================================================================
'
' Il produttore scrive sul canale a capacità 2 (make(chan T, 2)) e poi lo CHIUDE (close(ch)).
' Il consumatore riceve con l'OK flag (v, ok := <-ch): i valori buffered restano ricevibili
' dopo la chiusura; a buffer vuoto ok=0 e il loop termina.
'
' Output atteso:
'   OK 1 1
'   OK 2 1
'   OK -1 1
'   FINE 0
'   MAIN-DONE

SUB Produttore(C AS CHANNEL OF INTEGER (2))
  SEND 1 TO C
  SEND 2 TO C
  SEND -1 TO C
  CHANNEL CLOSE C
END SUB

SUB Consumatore(C AS CHANNEL OF INTEGER)
  DIM X AS LONG
  DIM OK AS LONG = 1
  RECEIVE X FROM C OK OK
  WHILE OK = 1
    PRINT "OK "; X; " "; OK
    RECEIVE X FROM C OK OK
  WEND
  PRINT "FINE "; OK
END SUB

SUB Main()
  DIM C AS CHANNEL OF INTEGER (2)
  DIM WG AS WAITGROUP
  GO SUB Produttore(C) IN WG
  GO SUB Consumatore(C) IN WG
  WAIT WG
  PRINT "MAIN-DONE"
END SUB
