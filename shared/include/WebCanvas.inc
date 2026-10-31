' FILE: include/WebCanvas.inc
' =================================================================================================
'   WEBCANVAS — bridge grafica remota (Canvas HTML5 via WebSocket)
' =================================================================================================
' Libreria di sistema PURA (BASIC, cross-backend: IL / PCodeVM / VM/C / C99).
' Serializza i comandi grafici (PSET/LINE/CLEAR/SPRITE) in JSON e li invia su un
' WEBSOCKET già aperto da un handler `HTTP ROUTE "GET","/ws/canvas",Handler(ws AS WEBSOCKET)`.
' Il browser esegue `webapp/static/canvas.js:CanvasReplay(cmd, canvas)` per disegnare.
'
' Design stateless: ogni SUB riceve il WS come primo parametro, così non serve stato
' globale (evita PRIVATE DIM WEBSOCKET che non è storabile in modo portabile).
'
' Uso:
'   IMPORT HTTP
'   IMPORT JSON
'   IMPORT WEBCANVAS
'
'   SUB CanvasWsHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE, ws AS WEBSOCKET)
'       ws.OnMessage = "CanvasOnMessage"
'       ws.OnClose = "CanvasOnClose"
'       WEBCANVAS.Clear(ws, 0)
'       WEBCANVAS.PSet(ws, 10, 10, 4)
'       WEBCANVAS.Line(ws, 0, 0, 100, 100, 7)
'       WEBCANVAS.Rect(ws, 20, 20, 60, 40, 2)
'       WEBCANVAS.Circle(ws, 80, 80, 20, 14)
'       WEBCANVAS.Sprite(ws, 3, 50, 80)
'       WEBCANVAS.Flush(ws)
'   END SUB
'
' Protocollo V1 (JSON testuale, editabile):
'   {"op":"pset","x":10,"y":10,"c":4}
'   {"op":"line","x1":0,"y1":0,"x2":100,"y2":100,"c":7}
'   {"op":"rect","x":20,"y":20,"w":60,"h":40,"c":2}
'   {"op":"circle","x":80,"y":80,"r":20,"c":14}
'   {"op":"paint","x":50,"y":50,"fc":3,"bc":0}
'   {"op":"putimage","x":10,"y":10,"w":32,"h":32,"data":[0,1,2,…],"mode":"PSET"}
'   {"op":"getimage","x":0,"y":0,"w":32,"h":32}
'   {"op":"sprite","id":3,"x":50,"y":80}
'   {"op":"clear","c":0}
'   {"op":"flush"}  + {"op":"text","x":10,"y":10,"txt":"Hello","c":7}
' =================================================================================================

IMPORT JSON

NAMESPACE WEBCANVAS

' --- Costanti editabili ---

PUBLIC CONST PROTOCOL_VERSION% = 1
PUBLIC CONST BINARY_MODE% = 0
PUBLIC CONST DEFAULT_WIDTH% = 800
PUBLIC CONST DEFAULT_HEIGHT% = 600

' --- Primitive (stateless: ws passato ogni volta) ---

PUBLIC SUB PSet(ws AS WEBSOCKET, x AS LONG, y AS LONG, col AS LONG)
    DIM j AS JSON = NEW JSON
    j["op"] = "pset"
    j["v"] = PROTOCOL_VERSION%
    j["x"] = x
    j["y"] = y
    j["c"] = col
    ws.Send(JSON.STRINGIFY(j))
END SUB

PUBLIC SUB Line(ws AS WEBSOCKET, x1 AS LONG, y1 AS LONG, x2 AS LONG, y2 AS LONG, col AS LONG)
    DIM j AS JSON = NEW JSON
    j["op"] = "line"
    j["v"] = PROTOCOL_VERSION%
    j["x1"] = x1
    j["y1"] = y1
    j["x2"] = x2
    j["y2"] = y2
    j["c"] = col
    ws.Send(JSON.STRINGIFY(j))
END SUB

PUBLIC SUB Rect(ws AS WEBSOCKET, x AS LONG, y AS LONG, w AS LONG, h AS LONG, col AS LONG)
    DIM j AS JSON = NEW JSON
    j["op"] = "rect"
    j["v"] = PROTOCOL_VERSION%
    j["x"] = x
    j["y"] = y
    j["w"] = w
    j["h"] = h
    j["c"] = col
    ws.Send(JSON.STRINGIFY(j))
END SUB

PUBLIC SUB Circle(ws AS WEBSOCKET, x AS LONG, y AS LONG, r AS LONG, col AS LONG)
    DIM j AS JSON = NEW JSON
    j["op"] = "circle"
    j["v"] = PROTOCOL_VERSION%
    j["x"] = x
    j["y"] = y
    j["r"] = r
    j["c"] = col
    ws.Send(JSON.STRINGIFY(j))
END SUB

PUBLIC SUB Sprite(ws AS WEBSOCKET, id AS LONG, x AS LONG, y AS LONG)
    DIM j AS JSON = NEW JSON
    j["op"] = "sprite"
    j["v"] = PROTOCOL_VERSION%
    j["id"] = id
    j["x"] = x
    j["y"] = y
    ws.Send(JSON.STRINGIFY(j))
END SUB

PUBLIC SUB TextOut(ws AS WEBSOCKET, x AS LONG, y AS LONG, txt AS STRING, col AS LONG)
    DIM j AS JSON = NEW JSON
    j["op"] = "text"
    j["v"] = PROTOCOL_VERSION%
    j["x"] = x
    j["y"] = y
    j["txt"] = txt
    j["c"] = col
    ws.Send(JSON.STRINGIFY(j))
END SUB

PUBLIC SUB Paint(ws AS WEBSOCKET, x AS LONG, y AS LONG, fillCol AS LONG, borderCol AS LONG)
    DIM j AS JSON = NEW JSON
    j["op"] = "paint"
    j["v"] = PROTOCOL_VERSION%
    j["x"] = x
    j["y"] = y
    j["fc"] = fillCol
    j["bc"] = borderCol
    ws.Send(JSON.STRINGIFY(j))
END SUB

PUBLIC SUB Clear(ws AS WEBSOCKET, col AS LONG)
    DIM j AS JSON = NEW JSON
    j["op"] = "clear"
    j["v"] = PROTOCOL_VERSION%
    j["c"] = col
    ws.Send(JSON.STRINGIFY(j))
END SUB

PUBLIC SUB Flush(ws AS WEBSOCKET)
    DIM j AS JSON = NEW JSON
    j["op"] = "flush"
    j["v"] = PROTOCOL_VERSION%
    ws.Send(JSON.STRINGIFY(j))
END SUB

PUBLIC SUB PutImage(ws AS WEBSOCKET, x AS LONG, y AS LONG, w AS LONG, h AS LONG, pixels AS ANY, mode AS STRING)
    DIM j AS JSON = NEW JSON
    j["op"] = "putimage"
    j["v"] = PROTOCOL_VERSION%
    j["x"] = x
    j["y"] = y
    j["w"] = w
    j["h"] = h
    j["data"] = pixels
    j["mode"] = mode
    ws.Send(JSON.STRINGIFY(j))
END SUB

PUBLIC SUB GetImage(ws AS WEBSOCKET, x AS LONG, y AS LONG, w AS LONG, h AS LONG)
    DIM j AS JSON = NEW JSON
    j["op"] = "getimage"
    j["v"] = PROTOCOL_VERSION%
    j["x"] = x
    j["y"] = y
    j["w"] = w
    j["h"] = h
    ws.Send(JSON.STRINGIFY(j))
END SUB

PUBLIC SUB Frame(ws AS WEBSOCKET, x AS LONG, y AS LONG, w AS LONG, h AS LONG, col AS LONG)
    Rect(ws, x, y, w, h, col)
END SUB
