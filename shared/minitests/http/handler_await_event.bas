' MiniTest §0ter: l'attesa dell'handler HTTP NON e' un ciclo di nanosleep.
'
' Il thread HTTP che consegna la risposta attende il completamento del TASK handler su una
' condizione (gmb_sched_wait_task_event, sveglia sull'evento con deadline), non con un poll da
' 500 µs. Il test lo esercita end-to-end: due richieste sequenziali a un handler LENTO (SLEEP 120
' dentro la SUB): ogni richiesta attraversa l'attesa piena, la risposta arriva integra, e la
' seconda prova che lo stato del task handler non resta appeso alla prima.
' EXPECT: start|[handler-ok]|[handler-ok]
IMPORT HTTP
IMPORT CONCURRENCY

SUB SlowHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  SLEEP 120
  res.Send("handler-ok")
END SUB

HTTP ROUTE "GET", "/slow", SlowHandler()

PRINT "start"
HTTP SERVER 8196

DIM wg AS WAITGROUP
GO SUB Client(wg) IN wg
WAIT wg
HTTP SERVER STOP
END

SUB Client(wg AS WAITGROUP)
  SLEEP 400
  PRINT "["; HTTP.GET("http://127.0.0.1:8196/slow"); "]"
  PRINT "["; HTTP.GET("http://127.0.0.1:8196/slow"); "]"
  DONE wg
END SUB
