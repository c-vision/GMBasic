' FILE: Http/BspModelAuthDemo.bas
' Dimostra MODEL e AUTHORIZE directives in BSP
' Avvio da REPL (dalla cartella di questo sample):
'   dotnet run --project <repo>/Executor -- -repl
'   > BSP ./pages/
'   > LOAD BspModelAuthDemo.bas
'   > RUN

IMPORT HTTP

DIM gUser AS JSON
gUser = NEW JSON
gUser("name") = ""
gUser("role") = ""

PRINT "=== BSP MODEL + AUTHORIZE Demo ==="

HTTP ROUTE "GET", "/login", LoginHandler()
HTTP ROUTE "GET", "/logout", LogoutHandler()
HTTP ROUTE "GET", "/api/status", StatusHandler()
HTTP ROUTE "GET", "/debug", DebugHandler()

DIM cfg AS HTTPCONFIG
cfg = NEW HTTPCONFIG
cfg.Port = 8095
HTTP CONFIG cfg

' Path RELATIVO alla cartella del sample (l'executor imposta la CWD sul sample, §9.1):
' portable, non più l'assoluto legacy GMBasic.
HTTP STATIC "/static", "wwwroot"

PRINT "Server on http://localhost:8095"
PRINT "  Pages: /dashboard_model.bsp, /profile.bsp, /admin.bsp"
PRINT "  Login: /login?u=name&r=role"
PRINT "  Logout: /logout"
PRINT

HTTP SERVER cfg.Port

PRINT "Running client test..."
DIM wg AS WAITGROUP
GO SUB ClientTest(wg) IN wg
WAIT wg
HTTP SERVER STOP

SUB LoginHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  gUser("name") = req.Query["u"]
  IF gUser("name") = "" THEN gUser("name") = "guest"

  gUser("role") = req.Query["r"]
  IF gUser("role") = "" THEN gUser("role") = "user"

  res.Send("<html><body style='font-family:sans-serif;max-width:600px;margin:2rem auto'>")
  res.Send("<h1>Logged in</h1>")
  res.Send("<p>User: <b>" + gUser("name") + "</b></p>")
  res.Send("<p>Role: <b>" + gUser("role") + "</b></p>")
  res.Send("<p><a href='/dashboard_model.bsp'>Dashboard</a></p>")
  res.Send("<p><a href='/profile.bsp'>Profile</a></p>")
  res.Send("<p><a href='/admin.bsp'>Admin Panel</a></p>")
  res.Send("</body></html>")
END SUB

SUB LogoutHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  gUser("name") = ""
  gUser("role") = ""
  res.Send("<html><body style='font-family:sans-serif;max-width:600px;margin:2rem auto'>")
  res.Send("<h1>Logged out</h1>")
  res.Send("<p><a href='/login'>Login again</a></p>")
  res.Send("</body></html>")
END SUB

SUB StatusHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  DIM s AS JSON
  s = NEW JSON
  s("user") = gUser("name")
  s("role") = gUser("role")
  res.SendJSON(s)
END SUB

SUB DebugHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  res.Send("<html><body><h1>Debug</h1><p>User: <b>" + gUser("name") + "</b>, Role: <b>" + gUser("role") + "</b></p></body></html>")
END SUB

SUB ClientTest(wg AS WAITGROUP)
  SLEEP 500
  DIM opts AS HTTPOPTIONS = NEW HTTPOPTIONS
  opts.Headers.Connection = "close"

  DIM r1 AS FETCHRESULT = FETCH("http://127.0.0.1:8095/login?u=alice&r=admin", opts)
  PRINT "[CLIENT] GET /login: "; r1.StatusCode

  DIM r2 AS FETCHRESULT = FETCH("http://127.0.0.1:8095/api/status", opts)
  PRINT "[CLIENT] GET /api/status: "; r2.StatusCode; " "; r2.Body

  DIM r3 AS FETCHRESULT = FETCH("http://127.0.0.1:8095/debug", opts)
  PRINT "[CLIENT] GET /debug: "; r3.StatusCode
END SUB
