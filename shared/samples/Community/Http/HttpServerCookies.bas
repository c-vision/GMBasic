' ==============================================================================
' Sample: HTTP SetCookie + HTTPCONFIG + Route parametrizzate
' ==============================================================================

IMPORT HTTP
SUB HelloHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  res.Send("<h1>Ciao da GM-BASIC!</h1>")
END SUB

SUB DataHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  res.SetCookie("session", "abc123")
  res.Send("ok")
END SUB

DIM cfg = NEW HTTPCONFIG
cfg.Port = 8080
HTTP CONFIG cfg
HTTP ROUTE "GET", "/hello", HelloHandler
HTTP ROUTE "GET", "/data", DataHandler
PRINT "Server sulla porta "; cfg.Port
