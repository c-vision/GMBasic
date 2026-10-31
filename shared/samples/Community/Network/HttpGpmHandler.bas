' FILE: Network/HttpGpmHandler.bas
' =================================================================================================
'       HTTP server: handler may-park (SLEEP + WAIT) sotto la GPM (parità PCodeVM §11.5)
' =================================================================================================
'
' Un handler che PARCHEGGIA (SLEEP) deve rispondere correttamente: il listener non esegue
' BASIC inline, l'handler gira come task GPM e la risposta è il future.
'
' Test: curl http://localhost:8156/test  → "GPM-OK"

IMPORT HTTP
IMPORT CONCURRENCY

SUB Worker(ch AS CHANNEL OF STRING)
  SLEEP 800
  SEND "from-goroutine" TO ch
END SUB

SUB DoGpm(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  DIM ch AS CHANNEL OF STRING
  DIM v AS STRING
  GO SUB Worker(ch)
  RECEIVE v FROM ch
  SLEEP 1500
  res.Send("GPM-OK-" + v)
END SUB

HTTP ROUTE "GET", "/test", DoGpm()
PRINT "Server on 8156"
HTTP SERVER 8156

DIM wg AS WAITGROUP
SLEEP 30000
PRINT "SERVER-DONE"
HTTP SERVER STOP
