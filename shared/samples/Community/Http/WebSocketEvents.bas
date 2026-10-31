IMPORT HTTP
IMPORT NET
' ==============================================================================
' Sample: WebSocket Events — OnMessage, OnClose, OnError, conversazione multi-round
' ==============================================================================
' Il server WebSocket con supporto eventi. Le proprietà OnMessage/OnClose/OnError
' del WEBSOCKET sono il NOME (stringa) di una SUB, richiamata rispettivamente a
' ogni messaggio ricevuto, alla chiusura pulita della connessione, e a un vero
' errore di connessione (socket rotto/reset, timeout — MAI a una chiusura
' pulita, che invoca solo OnClose) — non un riferimento a funzione, un binding
' per nome risolto a runtime.
'
' A differenza di un singolo scambio richiesta/risposta, qui il client manda
' PIÙ messaggi in sequenza sulla STESSA connessione (un contatore che cresce
' a ogni round, per verificare che il server risponda coerentemente messaggio
' per messaggio, non solo al primo) e chiude esplicitamente con un motivo
' (ws.Close(reason$) — il motivo non è oggi passato a OnClose lato server,
' vedi il commento sotto WsCloseHandler, ma è comunque parte dell'API client
' e utile da dimostrare).
'
' OnError NON viene esercitato qui: l'API client BASIC (WEBSOCKET CONNECT +
' ws.Close()) chiude sempre in modo pulito, per progetto — non offre un modo
' per forzare un reset di connessione grezzo. La callback è comunque registrata
' sotto per mostrare la sintassi; è stata verificata con un client TCP grezzo
' esterno che invia l'handshake e poi fa un RST invece del close frame pulito
' (vedi TODO-PCODE-BUGS.md per il dettaglio della verifica).
' ==============================================================================

IMPORT CONCURRENCY

DIM messageCount AS INTEGER

SUB WsMessageHandler(ws AS WEBSOCKET, msg$)
  messageCount = messageCount + 1
  PRINT "  [server] messaggio #"; messageCount; ": "; msg$
  ws.Send("Echo #" + STR$(messageCount) + ": " + msg$)
END SUB

SUB WsCloseHandler(ws AS WEBSOCKET)
  ' OnClose riceve solo il riferimento a ws, MAI il motivo passato da
  ' ws.Close(reason$) lato client — l'AST executor non propaga il motivo del
  ' frame CLOSE al server (vedi ExecuteHttpConnectionHandlerLoop case 11/12,
  ' Gpm.Http.Server.cs): il motivo esiste solo come stringa nel frame WebSocket
  ' sul wire, mai deserializzato lato server per l'evento OnClose.
  PRINT "  [server] connessione chiusa dopo "; messageCount; " messaggi totali."
END SUB

SUB WsErrorHandler(ws AS WEBSOCKET, errMsg$)
  ' Invocata SOLO per un errore reale di connessione (mai per una chiusura
  ' pulita) — subito seguita comunque da OnClose, per smontare la connessione.
  PRINT "  [server] errore di connessione: "; errMsg$
END SUB

SUB WsHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE, ws AS WEBSOCKET)
  PRINT "  [server] nuova connessione WebSocket!"
  ws.OnMessage = "WsMessageHandler"
  ws.OnClose = "WsCloseHandler"
  ws.OnError = "WsErrorHandler"
  ws.Send("Connesso al server WebSocket GM-BASIC!")
END SUB

HTTP ROUTE "GET", "/ws", WsHandler()

PRINT "Server WebSocket sulla porta 8099"
PRINT "Connettiti via: wscat -c ws://localhost:8099/ws"
HTTP SERVER 8099

DIM wg AS WAITGROUP
GO SUB ClientTest(wg) IN wg
WAIT wg
HTTP SERVER STOP

SUB ClientTest(wg AS WAITGROUP)
  SLEEP 300
  DIM ws AS WEBSOCKET
  WEBSOCKET CONNECT "127.0.0.1", 8099, ws, "/ws"
  SLEEP 200

  DIM i AS INTEGER
  FOR i = 1 TO 4
    ws.Send("messaggio numero " + STR$(i))
    SLEEP 150
  NEXT i

  ws.Close("conversazione terminata dal client")
  PRINT "[client] 4 messaggi inviati, connessione chiusa con motivo esplicito."
  DONE wg
END SUB
