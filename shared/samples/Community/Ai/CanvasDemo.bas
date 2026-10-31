' FILE: AI/CanvasDemo.bas
' =================================================================
' Sample per Canvas remoto (Web-Vision-Ext §1) — WebSocket + WebCanvas.inc
' Mostra il bridge WEBCANVAS (include/WebCanvas.inc) senza dashboard:
' avvia un WS su /ws/canvas e invia draw commands serializzati.
' Il browser con webapp/static/canvas.js li replaya su <canvas>.
'
' Avvio:
'   dotnet run --project REPL/REPL.csproj -- samples/Community/AI/CanvasDemo.bas
'   poi apri http://localhost:8092/canvas.bsp
' Oppure apri direttamente il canvas standalone (questa modalità):
'   curl http://localhost:8092/index.bsp
' =================================================================
IMPORT HTTP
IMPORT JSON
IMPORT WEBCANVAS

SUB Main()
    PRINT "=== CanvasDemo — WebSocket + WebCanvas.inc ==="
    PRINT "Include: " + "WEBCANVAS v" + LTRIM$(STR$(WEBCANVAS.PROTOCOL_VERSION%))
    PRINT

    DIM cfg AS HTTPCONFIG = NEW HTTPCONFIG
    cfg.Port = 8092
    HTTP CONFIG cfg
    HTTP STATIC "/", "./webapp/pages"
    HTTP STATIC "/static", "./webapp/static"
    HTTP ROUTE "GET", "/ws/canvas", CanvasHandler()

    PRINT "Canvas: http://localhost:8092/canvas.bsp"
    PRINT "WS:     ws://localhost:8092/ws/canvas"
    HTTP SERVER cfg.Port
    PRINT "Server attivo — apri il browser per vedere il canvas"
    SLEEP 5000
    HTTP SERVER STOP
    PRINT "Server fermato."
END SUB

SUB CanvasHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE, ws AS WEBSOCKET)
    ws.OnMessage = "OnWsMsg"
    ws.OnClose = "OnWsClose"
    ' WEBCANVAS.Init(ws) removed (stateless)
    WEBCANVAS.Clear(ws, 0)
    WEBCANVAS.PSet(ws, 10, 10, 4)
    WEBCANVAS.Line(ws, 0, 0, 120, 120, 7)
    WEBCANVAS.Rect(ws, 20, 20, 60, 40, 2)
    WEBCANVAS.Circle(ws, 80, 80, 20, 14)
    WEBCANVAS.TextOut(ws, 10, 150, "Hello Canvas", 7)
    WEBCANVAS.Flush(ws)
    PRINT "CanvasHandler: frame inviato"
END SUB

SUB OnWsMsg(ws AS WEBSOCKET, msg AS STRING)
    PRINT "[ws] msg: " + msg
END SUB

SUB OnWsClose(ws AS WEBSOCKET)
    PRINT "[ws] closed"
END SUB

