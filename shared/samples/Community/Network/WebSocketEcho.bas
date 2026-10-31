' ==============================================================================
' Sample: WebSocket — Echo server, PIÙ client concorrenti
' ==============================================================================
' Il server accetta connessioni WebSocket su /ws e fa da echo. La callback
' handler riceve (req, res, ws) dove ws è l'oggetto WEBSOCKET; ws.OnMessage
' è il nome (stringa) di una SUB richiamata a ogni messaggio ricevuto.
'
' A differenza della versione precedente (un solo client, un solo messaggio),
' questo sample avvia 3 client concorrenti (GOROUTINE, stesso pattern usato
' altrove per FETCH/HTTP — vedi Http/HttpFetchAdvanced.bas) per dimostrare che
' il dispatch WebSocket lato server non mescola i messaggi fra connessioni
' diverse: ogni client riceve SOLO gli echo dei propri messaggi, mai quelli
' degli altri due, anche se le connessioni sono aperte e attive nello stesso
' momento.
' ==============================================================================

IMPORT CONCURRENCY
IMPORT HTTP
IMPORT NET

SUB WsMessageHandler(ws AS WEBSOCKET, msg$)
  PRINT "  [server] ricevuto: "; msg$
  ws.Send("Echo: " + msg$)
END SUB

SUB WsHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE, ws AS WEBSOCKET)
  PRINT "  [server] nuova connessione: "; req.Path
  ws.OnMessage = "WsMessageHandler"
  ws.Send("Benvenuto sul WebSocket GM-BASIC!")
END SUB

HTTP ROUTE "GET", "/ws", WsHandler()

PRINT "Server WebSocket in ascolto sulla porta 8100"
PRINT "Connettiti via: wscat -c ws://localhost:8100/ws"
HTTP SERVER 8100

DIM wg AS WAITGROUP
GO SUB RunClient("A", wg) IN wg
GO SUB RunClient("B", wg) IN wg
GO SUB RunClient("C", wg) IN wg
WAIT wg
HTTP SERVER STOP

SUB RunClient(clientId$, wg AS WAITGROUP)
  SLEEP 300
  DIM ws AS WEBSOCKET
  WEBSOCKET CONNECT "127.0.0.1", 8100, ws, "/ws"
  SLEEP 200

  DIM i AS INTEGER
  FOR i = 1 TO 3
    ws.Send("client " + clientId$ + " msg " + STR$(i))
    SLEEP 150
  NEXT i

  ws.Close("client " + clientId$ + " ha finito")
  PRINT "[client "; clientId$; "] 3 messaggi inviati, connessione chiusa."
  DONE wg
END SUB
