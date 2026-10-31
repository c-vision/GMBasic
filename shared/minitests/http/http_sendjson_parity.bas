' Minitest HTTP — parità 4 backend: server + route + req.Query + res.SendJSON/ContentType.
IMPORT HTTP
SUB Main()
  DIM cfg AS HTTPCONFIG
  cfg = NEW HTTPCONFIG
  cfg.Port = 8797
  HTTP CONFIG cfg
  HTTP ROUTE "GET", "/j", JHandler()
  HTTP SERVER 8797
  SLEEP 800
  DIM r AS STRING
  r = HTTP.GET("http://localhost:8797/j?svc=alpha")
  PRINT "RESP=" + r
  HTTP SERVER STOP
END SUB
SUB JHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  DIM q AS STRING
  q = req.Query("svc")
  DIM o AS JSON
  o = NEW JSON
  o("svc") = q
  o("n") = 42
  res.ContentType = "application/json"
  res.SendJSON(o)
END SUB
