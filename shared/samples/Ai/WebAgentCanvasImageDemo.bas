' FILE: samples/AI/WebAgentCanvasImageDemo.bas
' =================================================================
' WEBAPP Canvas — demo dettagliata (nativo + WebSocket remoto + ImageGet/Put)
' Mostra tutte le 8 primitive remote: PSet/Line/Rect/Circle/Paint/PutImage/GetImage/Clear
' sia via GRAPHICS nativo (dual-write CurrentSink) sia via WEBCANVAS esplicito.
' =================================================================
IMPORT JSON
IMPORT WEBCANVAS

SUB CanvasWsHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE, ws AS WEBSOCKET)
    ' Handler per GET /ws/canvas — ogni SUB riceve ws già aperto
    ' Demo: disegna una scena completa e la invia al browser via WEBCANVAS (JSON)
    WEBCANVAS.Clear(ws, 0)
    WEBCANVAS.PSet(ws, 10, 10, 4)
    WEBCANVAS.Line(ws, 0, 0, 100, 100, 7)
    WEBCANVAS.Rect(ws, 20, 20, 60, 40, 2)
    WEBCANVAS.Circle(ws, 80, 80, 20, 14)
    WEBCANVAS.Paint(ws, 50, 50, 3, 0)
    WEBCANVAS.TextOut(ws, 10, 120, "Hello Canvas 8 primitive", 7)

    ' PutImage: sprite 4x4 con palette indices (demo: scacchiera)
    DIM sprite(18) AS LONG
    sprite(0)=4 : sprite(1)=4
    sprite(2)=1 : sprite(3)=2 : sprite(4)=1 : sprite(5)=2
    sprite(6)=2 : sprite(7)=1 : sprite(8)=2 : sprite(9)=1
    sprite(10)=1 : sprite(11)=2 : sprite(12)=1 : sprite(13)=2
    sprite(14)=2 : sprite(15)=1 : sprite(16)=2 : sprite(17)=1
    ' invio come PutImage: x=100,y=30,w=4,h=4, mode PSET
    WEBCANVAS.PutImage(ws, 100, 30, 4, 4, sprite, "PSET")

    ' GetImage: richiesta di cattura (JS logga, BASIC riceve buffer locale se SCREEN attivo)
    WEBCANVAS.GetImage(ws, 0, 0, 32, 32)

    WEBCANVAS.Flush(ws)
    PRINT "CanvasWsHandler: 8 primitive inviate (PSet/Line/Rect/Circle/Paint/Text/PutImage/GetImage/Clear)"
END SUB

SUB Main()
    PRINT "=== WEBAPP Canvas Image Demo (dettagliato) ==="
    PRINT ""

    ' ----------------------------------------------------------
    ' 1. Nativo: 8 primitive via CurrentSink (dual-write su WS se collegato)
    '    Quando webapp/app.bas imposta GraphicsManager.CurrentSink = WebSocketGraphicsSink(ws.Send),
    '    ogni comando grafico nativo fa anche CurrentSink?.PutImage/Paint/...
    '    Qui testiamo la via nativa (headless, dummy SDL) — non apre finestra.
    ' ----------------------------------------------------------
    PRINT "1) Nativo (headless): 8 primitive via GRAPHICS"
    PRINT "   SCREEN 320,200 — LINE/CIRCLE/PAINT — IMAGEGET/IMAGEPUT"
    ' Nota: SCREEN/IMAGEGET/IMAGEPUT sono comandi GRAPHICS (sintassi fused IMAGEGET/IMAGEPUT)
    ' Esempio nativo (se SDL disponibile):
    '   SCREEN 320, 200, 32
    '   LINE (10,10)-(50,50), 4
    '   CIRCLE (80,80), 20, 14
    '   PAINT (50,50), 3, 0
    '   DIM img() AS LONG
    '   IMAGEGET (10,10)-(50,50), img
    '   IMAGEPUT (100,100), img, "PSET"
    '   IMAGEPUT (120,120), img, "XOR"  ' AND/OR/XOR supportati
    PRINT "   (codice commentato per headless — scomenta con SDL attivo)"
    PRINT ""

    ' ----------------------------------------------------------
    ' 2. Remoto esplicito: WEBCANVAS (stateless, ws passato ogni volta)
    '    Protocollo V1 JSON in include/WebCanvas.inc → webapp/static/canvas.js:CanvasReplay
    '    Op: pset/line/rect/circle/paint/putimage/getimage/clear/flush/text/sprite
    '    Palette 16 colori CGA (0=nero ... 15=bianco) in canvas.js:GMB_PALETTE
    ' ----------------------------------------------------------
    PRINT "2) Remoto esplicito: WEBCANVAS (stateless)"
    PRINT "   WEBCANVAS.Clear(ws,0) → {""op"":""clear"",""c"":0}"
    PRINT "   WEBCANVAS.PSet(ws,10,10,4) → {""op"":""pset"",""x"":10,""y"":10,""c"":4}"
    PRINT "   WEBCANVAS.Line(ws,0,0,100,100,7) → {""op"":""line""...}"
    PRINT "   WEBCANVAS.Rect(ws,20,20,60,40,2) → {""op"":""rect""...}"
    PRINT "   WEBCANVAS.Circle(ws,80,80,20,14) → {""op"":""circle""...}"
    PRINT "   WEBCANVAS.Paint(ws,50,50,3,0) → {""op"":""paint"",""fc"":3,""bc"":0} + floodFill BFS in canvas.js"
    PRINT "   WEBCANVAS.PutImage(ws,100,30,4,4,data,""PSET"") → {""op"":""putimage"",""w"":4,""h"":4,""data"":[...]} + putImageData"
    PRINT "   WEBCANVAS.GetImage(ws,0,0,32,32) → {""op"":""getimage""} (JS capture log)"
    PRINT "   WEBCANVAS.TextOut(ws,10,120,""Hello"",7) → {""op"":""text""}"
    PRINT ""

    ' ----------------------------------------------------------
    ' 3. Server demo: avvio webapp completa (come webapp/app.bas)
    '    HTTP SERVER 8090 → pages/index.bsp (5 tab: Agents/Teams/Log/Canvas/WASM)
    '    + HTTP STATIC /static + WS /ws/canvas → CanvasWsHandler
    '    Il browser apre new WebSocket("ws://host/ws/canvas") e replaya i comandi.
    ' ----------------------------------------------------------
    PRINT "3) Server demo (come webapp/app.bas, senza avviare qui per headless)"
    PRINT "   HTTP ROUTE ""GET"", ""/ws/canvas"", CanvasWsHandler"
    PRINT "   HTTP SERVER 8090 + HTTP STATIC ""/static"" → canvas.js/wasm.js"
    PRINT "   Browser: new WebSocket(""ws://localhost:8090/ws/canvas"")"
    PRINT "   JS: CanvasReplay({op:""putimage"",...}, canvas) → palette + AND/OR/XOR"
    PRINT ""

    ' ----------------------------------------------------------
    ' 4. Palette e mode
    '    GMB_PALETTE 16 in canvas.js: 0=#000000 ... 7=#AAAAAA ... 15=#FFFFFF
    '    Mode IMAGEPUT: PSET (sovrascrive), AND/OR/XOR (su indice palette, poi ricalcola rgb)
    '    Trasparenza: indice <0 in PutImage → alfa 0 (pixel non disegnato)
    ' ----------------------------------------------------------
    PRINT "4) Palette 16 + mode"
    PRINT "   0 nero, 4 rosso, 7 grigio, 14 giallo, 15 bianco (vedi canvas.js:GMB_PALETTE)"
    PRINT "   Mode ""PSET"": pixel = data[i]"
    PRINT "   Mode ""AND"":  pixel = curIdx AND data[i]"
    PRINT "   Mode ""OR"":   pixel = curIdx OR  data[i]"
    PRINT "   Mode ""XOR"":  pixel = curIdx XOR data[i]  (per sprite con inversione)"
    PRINT "   Trasparente: data[i] = -1 → non disegnato"
    PRINT ""

    PRINT "Demo completata — apri webapp/pages/index.bsp → tab Canvas"
    PRINT "  Demo draw locale: CanvasReplay({op:""putimage""...}) senza server"
    PRINT "  Con server: webapp/app.bas → GET /ws/canvas → WEBCANVAS.PutImage/GetImage"
    PRINT "  Sorgente: include/WebCanvas.inc (protocollo JSON) + webapp/static/canvas.js (replay + floodFill)"
END SUB

