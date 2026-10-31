' PCodeVM: FETCH asincrona in goroutine contro un server HTTP locale. Con
' GOMAXPROCS=1 la FETCH deve parcheggiare il worker e riprendere col valore
' senza bloccare le altre goroutine (semantica Go, corpus P7).

IMPORT HTTP
IMPORT CONCURRENCY

SUB HelloHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  res.Send("CIAO-ESTERNO")
END SUB

HTTP ROUTE "GET", "/hello", HelloHandler()
HTTP SERVER 8101

SUB Fetcher(wg AS WAITGROUP)
  DIM r AS FETCHRESULT = FETCH("http://127.0.0.1:8101/hello")
  IF r.StatusCode = 200 AND r.Body = "CIAO-ESTERNO" THEN
    PRINT "FETCH-GOROUTINE-OK"
  ELSE
    PRINT "FAIL:FETCH-STATUS="; r.StatusCode; " BODY="; r.Body
  END IF
  DONE wg
END SUB

DIM wg AS WAITGROUP
GO SUB Fetcher(wg) IN wg
WAIT wg
HTTP SERVER STOP
PRINT "PCODE-GPM-HTTP-GOROUTINE-DONE"
