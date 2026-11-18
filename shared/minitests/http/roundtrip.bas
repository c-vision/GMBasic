' EXPECT: start|[GET-OK]|[ECHO:abc]|[ECHO:xyz]|[GET-OK]|status= 200|[GET-OK]
IMPORT HTTP
IMPORT CONCURRENCY

SUB GetHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  res.Send("GET-OK")
END SUB

SUB EchoHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  res.Send("ECHO:" + req.Body)
END SUB

HTTP ROUTE "GET", "/get", GetHandler()
HTTP ROUTE "POST", "/echo", EchoHandler()
HTTP ROUTE "PUT", "/echo", EchoHandler()
HTTP ROUTE "DELETE", "/del", GetHandler()

PRINT "start"
HTTP SERVER 8197

DIM wg AS WAITGROUP
GO SUB Client(wg) IN wg
WAIT wg
HTTP SERVER STOP
END

SUB Client(wg AS WAITGROUP)
  SLEEP 500
  PRINT "["; HTTP.GET("http://127.0.0.1:8197/get"); "]"
  PRINT "["; HTTP.POST("http://127.0.0.1:8197/echo", "abc"); "]"
  PRINT "["; HTTP.PUT("http://127.0.0.1:8197/echo", "xyz"); "]"
  PRINT "["; HTTP.DELETE("http://127.0.0.1:8197/del"); "]"
  DIM r AS FETCHRESULT = FETCH("http://127.0.0.1:8197/get")
  PRINT "status="; r.StatusCode
  PRINT "["; r.Body; "]"
END SUB
