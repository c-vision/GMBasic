' MiniTest NAMESPACE HTTP — tutte le 8 funzioni della superficie.
' DELETE, DETECTCONTENTTYPE, FETCH, GET, POST, PUT, SAFEFILENAME, SESSIONCREATE.
' Server HTTP locale su porta dedicata (18391) + client nella stessa gamba, come roundtrip.bas.
' EXPECT: start|get=[GET-OK]|post=[ECHO:abc]|put=[ECHO:xyz]|delete=[GET-OK]|fetch_status= 200|fetch_body=[GET-OK]|detect_gif=[image/gif]|detect_pdf=[application/pdf]|detect_none=[]|safe1=passwd.png|safe2=x.bin|safe3=report finale.PDF|session_name=-1|session_httponly=-1|session_samesite=-1
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

SUB Client(wg AS WAITGROUP)
  SLEEP 500
  PRINT "get=["; HTTP.GET("http://127.0.0.1:18391/get"); "]"
  PRINT "post=["; HTTP.POST("http://127.0.0.1:18391/echo", "abc"); "]"
  PRINT "put=["; HTTP.PUT("http://127.0.0.1:18391/echo", "xyz"); "]"
  PRINT "delete=["; HTTP.DELETE("http://127.0.0.1:18391/del"); "]"
  DIM r AS FETCHRESULT = FETCH("http://127.0.0.1:18391/get")
  PRINT "fetch_status="; r.StatusCode
  PRINT "fetch_body=["; r.Body; "]"
  PRINT "detect_gif=["; HTTP.DETECTCONTENTTYPE("GIF89a"); "]"
  PRINT "detect_pdf=["; HTTP.DETECTCONTENTTYPE("%PDF-1.4"); "]"
  PRINT "detect_none=["; HTTP.DETECTCONTENTTYPE("niente"); "]"
  PRINT "safe1="; HTTP.SAFEFILENAME("../../etc/passwd.png")
  PRINT "safe2="; HTTP.SAFEFILENAME("C:\temp\..\x.bin")
  PRINT "safe3="; HTTP.SAFEFILENAME("report finale.PDF")
  DIM ck AS STRING = HTTP.SESSIONCREATE("demo", "admin")
  PRINT "session_name="; (INSTR(ck, "GMBSESSION=") > 0)
  PRINT "session_httponly="; (INSTR(ck, "HttpOnly") > 0)
  PRINT "session_samesite="; (INSTR(ck, "SameSite=Lax") > 0)
END SUB

SUB Main()
  PRINT "start"
  HTTP SERVER 18391
  DIM wg AS WAITGROUP
  GO SUB Client(wg) IN wg
  WAIT wg
  HTTP SERVER STOP
END SUB
