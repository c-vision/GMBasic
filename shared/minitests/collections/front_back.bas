' EXPECT: 1|2|count= 2
IMPORT COLLECTIONS
SUB Main()
  ' QUEUE Front/Back: front = testa (prossimo da Dequeue), back = coda.
  DIM q AS QUEUE OF STRING = NEW QUEUE OF STRING
  q.Enqueue("1") : q.Enqueue("2")
  PRINT q.Front()
  PRINT q.Back()
  PRINT "count="; q.Count()
END SUB
