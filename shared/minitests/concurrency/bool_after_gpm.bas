' EXPECT: dopo_wait=-1
' Un confronto materializzato come VALORE dentro un metodo lowered come coroutine (Main che
' contiene GO SUB) deve valere -1, il canone booleano BASIC.
' Il path FLAT ottiene -1 emettendo OpCodes.Neg dopo il confronto; il path coroutine
' (EmitBinaryDouble in IlCoroutineEmitter.MethodEmitter.Control.cs) spingeva invece 1.0, quindi
' su IL i programmi con concorrenza stampavano 1 dove P-Code e VM/C stampavano -1.
' Non serve rete: il trigger e' la presenza di GPM nel programma, non il suspend/resume
' (stessa divergenza anche calcolando il confronto PRIMA del GO SUB).
IMPORT CONCURRENCY

DIM gFlag AS INTEGER

ASYNC SUB Worker(wg AS WAITGROUP)
  gFlag = 1
  DONE wg
END SUB

SUB Main()
  DIM wg AS WAITGROUP
  GO SUB Worker(wg) IN wg
  WAIT wg
  DIM f AS INTEGER
  f = LEN("x") > 0
  PRINT "dopo_wait="; f
END SUB