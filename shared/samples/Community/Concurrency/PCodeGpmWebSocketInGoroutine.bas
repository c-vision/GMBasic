' PCodeVM: WebSocket in goroutine con lifecycle completo (parità GPM).
' Il server fa da echo; due client in goroutine si connettono, inviano e il
' server risponde. Il park WS (send/receive async) non blocca il worker: con
' GOMAXPROCS=1 l'intero round-trip sta su un solo worker. Verifica che
' connessione, invio, ricezione e chiusura completino senza hang (CTS 30s).
' Nota: HTTP ROUTE / HTTP SERVER vivono a livello module (pattern canonico).

IMPORT CONCURRENCY
IMPORT HTTP
IMPORT NET

SUB WsMessageHandler(ws AS WEBSOCKET, msg$)
  ' Echo server: risponde con lo stesso messaggio prefissato.
  ws.Send("ECHO: " + msg$)
  PRINT "SERVER-RECV-"; msg$
END SUB

SUB WsHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE, ws AS WEBSOCKET)
  ws.OnMessage = "WsMessageHandler"
END SUB

HTTP ROUTE "GET", "/ws", WsHandler()

SUB Client(Id AS INTEGER, WG AS WAITGROUP)
  DIM ws AS WEBSOCKET
  SLEEP 200
  WEBSOCKET CONNECT "127.0.0.1", 18823, ws, "/ws"
  IF ws IS NOTHING THEN
    PRINT "FAIL:WS-CONNECT-"; Id
    DONE WG
    RETURN
  END IF
  ws.Send("msg-" + STR$(Id))
  SLEEP 300
  ws.Close("done")
  PRINT "WS-CLIENT-"; Id; "-OK"
  DONE WG
END SUB

HTTP SERVER 18823
PRINT "SERVER-STARTED"
SLEEP 500

DIM wg AS WAITGROUP
GO SUB Client(1, wg) IN wg
GO SUB Client(2, wg) IN wg
WAIT wg
HTTP SERVER STOP
PRINT "PCODE-GPM-WS-GOROUTINE-DONE"
