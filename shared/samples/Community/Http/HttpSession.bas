' ==============================================================================
' Sample: Session tracking — req.SessionId
' ==============================================================================
' Il server HTTP estrae automaticamente il cookie GM_BASIC_SESSION dalla
' richiesta e lo espone come req.SessionId.
'
' Il comportamento session-less del browser può essere simulato con curl:
'   curl -v http://localhost:8097/
'   curl -v -b "GM_BASIC_SESSION=abc123" http://localhost:8097/
' ==============================================================================

IMPORT CONCURRENCY
IMPORT HTTP

SUB IndexHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  DIM sess = req.SessionId

  IF sess = "" THEN
    ' Nuova visita — imposta un cookie di sessione
    DIM nuovoId = STR$(INT(RND() * 1000000))
    res.SetCookie("GM_BASIC_SESSION", nuovoId)
    res.Send("<h1>Benvenuto!</h1><p>Cookie di sessione impostato.</p>")
  ELSE
    res.Send("<h1>Ritorno!</h1><p>Session ID: " + sess + "</p>")
  END IF
END SUB

SUB DataHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  DIM risposta AS JSON = NEW JSON
  risposta.sessionId = req.SessionId
  risposta.path = req.Path
  risposta.method = req.Method
  risposta.ip = req.RemoteIp
  risposta.timestamp = NOW()
  res.SendJSON(risposta)
END SUB

HTTP ROUTE "GET", "/", IndexHandler()
HTTP ROUTE "GET", "/api/info", DataHandler()

PRINT "Server sulla porta 8097"
PRINT "Endpoint: GET / e GET /api/info"
HTTP SERVER 8097

DIM wg AS WAITGROUP
GO SUB ClientTest(wg) IN wg
WAIT wg
HTTP SERVER STOP

SUB ClientTest(wg AS WAITGROUP)
  SLEEP 300
  DIM opts AS HTTPOPTIONS = NEW HTTPOPTIONS
  opts.Headers.Connection = "close"
  DIM homeRes AS FETCHRESULT = FETCH("http://127.0.0.1:8097/", opts)
  PRINT "[CLIENT] GET /: "; homeRes.StatusCode; " "; homeRes.Body
  DIM infoRes AS FETCHRESULT = FETCH("http://127.0.0.1:8097/api/info", opts)
  PRINT "[CLIENT] GET /api/info: "; infoRes.StatusCode; " "; infoRes.Body
END SUB
