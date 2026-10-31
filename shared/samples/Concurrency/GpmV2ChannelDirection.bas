' G8-bis (piano ILBackend-GPM): canali direzionali (parità Go chan<- T / <-chan T) sotto la v2.
' Un producer invia su un canale SendOnly; un consumer riceve da un canale ReceiveOnly.
' Il canale viene passato ai worker con la direzione nel tipo del parametro. Il runtime è
' invariato; la direzione è un contratto di tipo (SEND su ReceiveOnly / RECEIVE su SendOnly
' sono errori di compilazione). Totale deterministico: 1+2+3 = 6.

SUB Producer(sendCh AS CHANNEL SEND OF INTEGER, wg AS WAITGROUP)
  DIM i AS INTEGER
  FOR i = 1 TO 3
    SEND i TO sendCh
  NEXT i
  CHANNEL CLOSE sendCh
END SUB

SUB Consumer(recvCh AS CHANNEL RECEIVE OF INTEGER, wg AS WAITGROUP)
  DIM v AS INTEGER
  DIM i AS INTEGER
  DIM ok AS BIT
  DIM Total AS LONG = 0
  FOR i = 1 TO 3
    RECEIVE v FROM recvCh OK ok
    Total = Total + v
  NEXT i
  IF Total = 6 THEN
    PRINT "GPM-V2-CHAN-DIR-OK"
  ELSE
    PRINT "FAIL:TOTAL="; Total
  END IF
END SUB

SUB Main()
  DIM ch AS CHANNEL OF INTEGER
  DIM wg AS WAITGROUP
  GO SUB Producer(ch, wg) IN wg
  GO SUB Consumer(ch, wg) IN wg
  WAIT wg
  PRINT "GPM-V2-CHAN-DIR-DONE"
END SUB
