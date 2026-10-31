' ==============================================================================
' Sample: HTTP Full Server — tutte le funzionalità
' ==============================================================================

IMPORT CONCURRENCY
IMPORT HTTP

SUB IndexHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  res.Send("<h1>GM-BASIC Web Server</h1>")
END SUB

SUB UserHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  DIM p = req.Params
  DIM id = p("id")
  res.Send("Utente ID: " + id)
END SUB

SUB JsonHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  res.SetCookie("lastAccess", STRTIME())
  res.Send("JSON ricevuto: " + req.Body)
END SUB

' Config
DIM cfg AS HTTPCONFIG = NEW HTTPCONFIG
cfg.Port = 8093
cfg.RequestTimeoutMs = 10000
HTTP CONFIG cfg

' Routes
HTTP ROUTE "GET", "/", IndexHandler()
HTTP ROUTE "GET", "/users/{id}", UserHandler()
HTTP ROUTE "POST", "/api/json", JsonHandler()

' Static files
HTTP STATIC "/static", "./wwwroot"

PRINT "=== GM-BASIC HTTP Server ==="
PRINT "Porta: "; cfg.Port
PRINT "Endpoint: GET /, GET /users/{id}, POST /api/json, GET /static/*"
HTTP SERVER cfg.Port

DIM wg AS WAITGROUP
GO SUB ClientTest(wg) IN wg
WAIT wg
HTTP SERVER STOP

SUB ClientTest(wg AS WAITGROUP)
  SLEEP 300
  DIM opts AS HTTPOPTIONS = NEW HTTPOPTIONS
  opts.Headers.Connection = "close"

  DIM homeRes AS FETCHRESULT = FETCH("http://127.0.0.1:8093/", opts)
  PRINT "[CLIENT] GET /: "; homeRes.StatusCode; " "; homeRes.Body

  DIM userRes AS FETCHRESULT = FETCH("http://127.0.0.1:8093/users/42", opts)
  PRINT "[CLIENT] GET /users/42: "; userRes.StatusCode; " "; userRes.Body

  DIM jsonOpts AS HTTPOPTIONS = NEW HTTPOPTIONS
  jsonOpts.Method = "POST"
  jsonOpts.Body = "{""nome"":""test""}"
  jsonOpts.Headers.ContentType = "application/json"
  jsonOpts.Headers.Connection = "close"
  DIM jsonRes AS FETCHRESULT = FETCH("http://127.0.0.1:8093/api/json", jsonOpts)
  PRINT "[CLIENT] POST /api/json: "; jsonRes.StatusCode; " "; jsonRes.Body

  DIM staticRes AS FETCHRESULT = FETCH("http://127.0.0.1:8093/static/index.html", opts)
  PRINT "[CLIENT] GET /static/index.html: "; staticRes.StatusCode; " "; staticRes.Body
END SUB
