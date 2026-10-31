' ==============================================================================
' Sample: req.BodyJson — Parsing automatico JSON body
' ==============================================================================
' Dimostra l'auto-parsing JSON del corpo della richiesta.
' Quando il Content-Type è application/json, req.BodyJson viene popolato
' automaticamente dal server HTTP.
'
' Endpoint:
'   POST /api/data   -> riceve JSON, legge req.BodyJson
'
' Il test è autonomo: avvia il server e, in una goroutine separata, un client
' che esegue la richiesta di esempio e poi ferma il server. Così il programma
' termina da solo invece di restare in ascolto per sempre.
' ==============================================================================

IMPORT CONCURRENCY
IMPORT HTTP

SUB DataHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  ' req.BodyJson è automaticamente parsato se Content-Type: application/json
  DIM j = req.BodyJson

  ' Legge campi dal JSON
  DIM nome = j("nome")
  DIM valore = j("valore")
  PRINT "Ricevuto: nome="; nome; " valore="; valore

  ' Risponde con JSON
  DIM risposta AS JSON = NEW JSON
  risposta.status = "ok"
  risposta.ricevuto = nome
  risposta.timestamp = NOW()
  res.SendJSON(risposta)
END SUB

' --- Route ---
HTTP ROUTE "POST", "/api/data", DataHandler()

' --- Avvia il server ---
PRINT "Server in ascolto sulla porta 8091"
PRINT "Invia una POST a http://localhost:8091/api/data con JSON body"
PRINT '  Esempio: curl -X POST -H "Content-Type: application/json" -d "{\"nome\":\"test\",\"valore\":42}" http://localhost:8091/api/data'
HTTP SERVER 8091

' --- Client di test autonomo ---
DIM wg AS WAITGROUP
GO SUB ClientTest(wg) IN wg
WAIT wg
HTTP SERVER STOP

SUB ClientTest(wg AS WAITGROUP)
  SLEEP 300
  DIM opts AS HTTPOPTIONS = NEW HTTPOPTIONS
  opts.Method = "POST"
  opts.Body = "{""nome"":""test"",""valore"":42}"
  opts.Headers.ContentType = "application/json"
  opts.Headers.Connection = "close"
  DIM res AS FETCHRESULT = FETCH("http://127.0.0.1:8091/api/data", opts)
  PRINT "[CLIENT] Status: "; res.StatusCode; " Body: "; res.Body
END SUB
