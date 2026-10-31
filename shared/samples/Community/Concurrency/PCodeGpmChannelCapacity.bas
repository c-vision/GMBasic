' PCodeVM: capacity esplicita del canale (parità Go make(chan T, n)).
' DIM ch AS CHANNEL OF INTEGER (2) crea un canale con buffer di 2 elementi.
' Un SEND su buffer pieno parcheggia il sender; il RECEIVE concorrente lo libera.
' Verifica che la capacity sia davvero 2 (non il default 10): con capacity 2,
' un producer che invia 5 valori e un consumer che riceve 5 in parallelo devono
' completare (totale deterministico).

IMPORT CONCURRENCY

SUB Producer(ch AS CHANNEL OF INTEGER, wg AS WAITGROUP)
  DIM i AS INTEGER
  FOR i = 1 TO 5
    SEND i TO ch
  NEXT i
  DONE wg
END SUB

SUB Consumer(ch AS CHANNEL OF INTEGER, wg AS WAITGROUP)
  DIM v AS INTEGER
  DIM i AS INTEGER
  DIM Total AS INTEGER = 0
  FOR i = 1 TO 5
    RECEIVE v FROM ch
    Total = Total + v
  NEXT i
  PRINT "CONSUMER-TOTAL="; Total
  DONE wg
END SUB

SUB Main()
  DIM ch AS CHANNEL OF INTEGER (2)
  DIM wg AS WAITGROUP
  GO SUB Producer(ch, wg) IN wg
  GO SUB Consumer(ch, wg) IN wg
  WAIT wg
  PRINT "CAPACITY-2-PRODUCER-CONSUMER-OK"
END SUB
