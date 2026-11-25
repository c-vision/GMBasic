' MiniTest: la 404 senza route deve avere il corpo CANONICO "Not Found" (9 byte) su
' tutte le gambe. Prima: IL "Not Found", PCode "404 Not Found" (13), VM/C e C99 vuoto.
' EXPECT: st= 404|len= 9|body=[Not Found]
IMPORT HTTP

SUB H(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  res.Send("ok")
END SUB

HTTP ROUTE "GET", "/ok", H()
HTTP SERVER 8210

DIM wg AS WAITGROUP
GO SUB C(wg) IN wg
WAIT wg
HTTP SERVER STOP
END

SUB C(wg AS WAITGROUP)
  SLEEP 500
  DIM r AS FETCHRESULT = FETCH("http://127.0.0.1:8210/nope")
  PRINT "st="; r.StatusCode
  PRINT "len="; LEN(r.Body)
  PRINT "body=["; r.Body; "]"
  DONE wg
END SUB
