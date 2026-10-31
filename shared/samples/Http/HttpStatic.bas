' ==============================================================================
' Sample: HTTP STATIC — Servire file statici
' ==============================================================================
' Registra un percorso URL come servizio di file statici.
' I file nella directory fisica vengono serviti automaticamente.
'
' Endpoint:
'   GET /static/index.html  -> serve ./wwwroot/index.html
'   GET /static/style.css   -> serve ./wwwroot/style.css
' ==============================================================================

IMPORT CONCURRENCY
IMPORT HTTP

SUB HomeHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  res.Send("<h1>GM-BASIC Web Server</h1><p><a href='/static/index.html'>Vai alla pagina statica</a></p>")
END SUB

' --- Registra il percorso statico ---
' Tutte le richieste a /static/* vengono servite dalla directory ./wwwroot
HTTP STATIC "/static", "./wwwroot"

' --- Route normale ---
HTTP ROUTE "GET", "/", HomeHandler()

' --- Avvia il server ---
DIM cfg AS HTTPCONFIG = NEW HTTPCONFIG
cfg.Port = 8098
HTTP CONFIG cfg
PRINT "Server avviato sulla porta "; cfg.Port
PRINT "File statici disponibili su http://localhost:8098/static/"
HTTP SERVER cfg.Port

DIM wg AS WAITGROUP
GO SUB ClientTest(wg) IN wg
WAIT wg
HTTP SERVER STOP

SUB ClientTest(wg AS WAITGROUP)
  SLEEP 300
  DIM opts AS HTTPOPTIONS = NEW HTTPOPTIONS
  opts.Headers.Connection = "close"

  DIM homeRes AS FETCHRESULT = FETCH("http://127.0.0.1:8098/", opts)
  PRINT "[CLIENT] GET /: "; homeRes.StatusCode; " "; homeRes.Body

  DIM htmlRes AS FETCHRESULT = FETCH("http://127.0.0.1:8098/static/index.html", opts)
  PRINT "[CLIENT] GET /static/index.html: "; htmlRes.StatusCode

  DIM cssRes AS FETCHRESULT = FETCH("http://127.0.0.1:8098/static/style.css", opts)
  PRINT "[CLIENT] GET /static/style.css: "; cssRes.StatusCode
END SUB
