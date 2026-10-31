' ==============================================================================
' Sample: req.ParseForm() — Form url-encoded parsing
' ==============================================================================
' Il metodo req.ParseForm() analizza il body come application/x-www-form-urlencoded
' e restituisce un oggetto JSON con i campi del form.
'
' Il test è autonomo: avvia il server e, in una goroutine separata, un client
' che esegue la richiesta di esempio e poi ferma il server.
' ==============================================================================

IMPORT CONCURRENCY
IMPORT HTTP

SUB FormHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  PRINT "Form ricevuto!"
  PRINT "Body: "; req.Body
  res.Send("OK")
END SUB

HTTP ROUTE "POST", "/form", FormHandler()

PRINT "Server sulla porta 8092"
PRINT 'Invia: curl -X POST -d "nome=Mario&email=mario@test.com" http://localhost:8092/form'
HTTP SERVER 8092

DIM wg AS WAITGROUP
GO SUB ClientTest(wg) IN wg
WAIT wg
HTTP SERVER STOP

SUB ClientTest(wg AS WAITGROUP)
  SLEEP 300
  DIM opts AS HTTPOPTIONS = NEW HTTPOPTIONS
  opts.Method = "POST"
  opts.Body = "nome=Mario&email=mario@test.com"
  opts.Headers.ContentType = "application/x-www-form-urlencoded"
  opts.Headers.Connection = "close"
  DIM res AS FETCHRESULT = FETCH("http://127.0.0.1:8092/form", opts)
  PRINT "[CLIENT] Status: "; res.StatusCode; " Body: "; res.Body
END SUB
