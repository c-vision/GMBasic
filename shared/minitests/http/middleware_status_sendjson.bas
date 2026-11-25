' MiniTest HTTP — middleware che BLOCCA con StatusCode 401 + SendJSON(dict), e
' res.SendJSON(<TESTO JSON>) che deve INCAPSULARE la stringa (body quotato).
' Prima (gamba IL) `res.StatusCode = 401` nel middleware era un no-op silenzioso
' (setter privato) → 200; e su C99 la catena `d.Values().ToJson()` perdeva il ToJson
' → body `[]` invece di `"[]"`.
' EXPECT: noauth= 401 |{"error":"no"}|auth= 200 |"[]"
IMPORT HTTP

SUB AuthMiddleware(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  DIM token AS STRING
  token = req.Headers.Get("X-Auth-Token")
  IF token = "ok" THEN
    HTTP USE NEXT
  ELSE
    res.StatusCode = 401
    res.SendJSON({"error": "no"})
  END IF
END SUB

SUB EmptyListHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  DIM d AS DICT
  res.SendJSON(d.Values().ToJson())
END SUB

HTTP USE "/api", AuthMiddleware()
HTTP ROUTE "GET", "/api/x", EmptyListHandler()
HTTP SERVER 8198

DIM wg AS WAITGROUP
GO SUB Client(wg) IN wg
WAIT wg
HTTP SERVER STOP
END

SUB Client(wg AS WAITGROUP)
  SLEEP 500
  DIM r1 AS FETCHRESULT = FETCH("http://127.0.0.1:8198/api/x")
  PRINT "noauth="; r1.StatusCode; "|"; r1.Body
  DIM o AS HTTPOPTIONS = NEW HTTPOPTIONS
  o.Headers.Add("X-Auth-Token", "ok")
  DIM r2 AS FETCHRESULT = FETCH("http://127.0.0.1:8198/api/x", o)
  PRINT "auth="; r2.StatusCode; "|"; r2.Body
  DONE wg
END SUB
