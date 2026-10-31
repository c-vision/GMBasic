' FILE: webapp/app.bas — Launcher unificato WEBAPP GMBasic (C1b + Canvas + WASM)
' =============================================================================
' Avvia la WEBAPP Agents Dashboard (C1b, 5 tab) + Canvas WS + WASM shell.
' Tutto in BASIC: seed agenti/team demo, HTTP SERVER + BSP, proxy /api same-origin,
' WebSocket /ws/canvas con WEBCANVAS.
'
' Porte (editabili):
'   API agents: 8765 (AI AGENT SERVE, route C1 /api/agents|teams|/agent/{n}/prompt)
'   Dashboard:  8090 (HTTP SERVER, serve webapp/pages/*.bsp + webapp/static/*)
'
' Avvio:
'   dotnet run --project REPL/REPL.csproj -- webapp/app.bas
'   poi apri http://localhost:8090/index.bsp  (o /canvas.bsp, /wasm.bsp)
'   API diretta: curl http://localhost:8765/api/agents
'
' Editabilità: l'utente modifica qui porte, seed, route proxy, handler WS, senza toccare C#.
' Il file è il launcher di riferimento; i sample in samples/Community/AI/* sono copie minimali.
' =============================================================================

IMPORT HTTP
IMPORT JSON
IMPORT AI WITH ALIAS
IMPORT DASHBOARD
IMPORT WEBCANVAS

' --- Seed demo (se registry vuoto) ---
SUB SeedDemo()
    DIM names() = AI.AGENTLIST()
    IF UBOUND(names) < 0 THEN
        PRINT "Seed: creo agenti demo..."
        AI AGENT CREATE "Demo"
        AI AGENT SET "Demo" systemPrompt "You are a helpful demo agent."
        AI AGENT SET "Demo" permission YOLO
        AI AGENT CREATE "Analyst"
        AI AGENT SET "Analyst" systemPrompt "You are a concise analyst."
        AI AGENT SET "Analyst" temperature 0.3
        AI AGENT CREATE "Support"
        AI AGENT SET "Support" systemPrompt "You are support."
        PRINT "Seed: 3 agenti creati (Demo, Analyst, Support)"
    END IF
END SUB

SUB Main()
    PRINT "=== GMBasic WEBAPP — Agents + Canvas + WASM ==="
    PRINT

    SeedDemo()
    AI AGENT TRACE VERBOSE
    PRINT "TRACE VERBOSE attivato (eventi nel Log)"

    ' --- 1. Avvia Agent API server (C1, route JSON) ---
    DIM apiPort% = 8765
    AI AGENT SERVE apiPort%
    PRINT "Agent API: http://localhost:" + LTRIM$(STR$(apiPort%)) + "/api/agents  (GET)"
    PRINT "           http://localhost:" + LTRIM$(STR$(apiPort%)) + "/api/teams   (GET)"
    PRINT "           http://localhost:" + LTRIM$(STR$(apiPort%)) + "/api/agent/Demo/prompt  (POST {prompt})"
    PRINT

    ' --- 2. Avvia Dashboard server (BSP + static) ---
    DIM cfg AS HTTPCONFIG = NEW HTTPCONFIG
    cfg.Port = 8090
    cfg.Sessions = TRUE
    HTTP CONFIG cfg

    ' Serve BSP e statici: webapp/pages/*.bsp → renderizzate, webapp/static/* → static
    HTTP STATIC "/", "./webapp/pages"
    HTTP STATIC "/static", "./webapp/static"

    ' --- 3. Proxy same-origin /api/* → 8765 (evita CORS per JS fetch("/api/agents")) ---
    ' Il JS di index.bsp fa fetch("/api/agents") same-origin; il BASIC lo proxya alla API.
    HTTP ROUTE "GET", "/api/agents", ProxyAgents()
    HTTP ROUTE "GET", "/api/teams", ProxyTeams()
    HTTP ROUTE "POST", "/api/agent/{name}/prompt", ProxyPrompt()
    HTTP ROUTE "GET", "/api/events", ProxyEvents()

    ' --- 4. WebSocket Canvas (Vision-Ext §1) ---
    HTTP ROUTE "GET", "/ws/canvas", CanvasWsHandler()

    PRINT "Dashboard: http://localhost:" + LTRIM$(STR$(cfg.Port)) + "/index.bsp  (5 tab: Agents/Teams/Log/Canvas/WASM)"
    PRINT "Canvas:    http://localhost:" + LTRIM$(STR$(cfg.Port)) + "/canvas.bsp"
    PRINT "WASM:      http://localhost:" + LTRIM$(STR$(cfg.Port)) + "/wasm.bsp"
    PRINT "API proxy: http://localhost:" + LTRIM$(STR$(cfg.Port)) + "/api/agents  (proxato → :" + LTRIM$(STR$(apiPort%)) + ")"
    PRINT
    PRINT "CTRL+C per fermare"
    PRINT

    HTTP SERVER cfg.Port

    ' Tieni vivo (il server gira in background, GPM park-based)
    SLEEP 3600000

    HTTP SERVER STOP
    AI AGENT STOP SERVER
    PRINT "Server fermati."
END SUB

' --- Proxy handlers (BASIC, editabili) ---

SUB ProxyAgents(req AS HTTPREQUEST, res AS HTTPRESPONSE)
    DIM r AS FETCHRESULT = FETCH("http://127.0.0.1:8765/api/agents")
    res.Send(r.Body)
END SUB

SUB ProxyTeams(req AS HTTPREQUEST, res AS HTTPRESPONSE)
    DIM r AS FETCHRESULT = FETCH("http://127.0.0.1:8765/api/teams")
    res.Send(r.Body)
END SUB

SUB ProxyEvents(req AS HTTPREQUEST, res AS HTTPRESPONSE)
    DIM r AS FETCHRESULT = FETCH("http://127.0.0.1:8765/api/events")
    IF r.StatusCode = 200 THEN
        res.Send(r.Body)
    ELSE
        DIM evs() = AI.AGENTEVENTS()
        DIM sb$ = "["
        FOR idx = 0 TO UBOUND(evs)
            IF idx > 0 THEN sb$ = sb$ + ","
            DIM d$ = evs(idx)
            d$ = REPLACE$(d$, "\", "\\")
            d$ = REPLACE$(d$, CHR$(34), "\" + CHR$(34))
            d$ = REPLACE$(d$, CHR$(10), "\n")
            sb$ = sb$ + "{" + CHR$(34) + "detail" + CHR$(34) + ":" + CHR$(34) + d$ + CHR$(34) + "}"
        NEXT idx
        sb$ = sb$ + "]"
        res.Send(sb$)
    END IF
END SUB

SUB ProxyPrompt(req AS HTTPREQUEST, res AS HTTPRESPONSE)
    DIM name$ = req.Params["name"]
    DIM body$ = req.Body
    DIM r AS FETCHRESULT = FETCH("http://127.0.0.1:8765/api/agent/" + name$ + "/prompt", NEW HTTPOPTIONS)
    ' FETCH con POST richiede HTTPOPTIONS — fallback: usa POST helper se body già JSON
    ' Per semplicità, re-invia con POST
    DIM opts AS HTTPOPTIONS = NEW HTTPOPTIONS
    opts.Method = "POST"
    opts.Body = body$
    opts.Headers.Add("Content-Type", "application/json")
    DIM r2 AS FETCHRESULT = FETCH("http://127.0.0.1:8765/api/agent/" + name$ + "/prompt", opts)
    res.Send(r2.Body)
END SUB

' --- WebSocket Canvas handler (Vision-Ext §1) ---

SUB CanvasWsHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE, ws AS WEBSOCKET)
    ws.OnMessage = "CanvasOnMessage"
    ws.OnClose = "CanvasOnClose"
    ' WEBCANVAS.Init(ws) removed (stateless)
    ' demo frame iniziale
    WEBCANVAS.Clear(ws, 0)
    WEBCANVAS.PSet(ws, 10, 10, 4)
    WEBCANVAS.Line(ws, 0, 0, 120, 120, 7)
    WEBCANVAS.Rect(ws, 20, 20, 60, 40, 2)
    WEBCANVAS.Circle(ws, 80, 80, 20, 14)
    WEBCANVAS.TextOut(ws, 10, 150, "Hello Canvas", 7)
    WEBCANVAS.Flush(ws)
END SUB

SUB CanvasOnMessage(ws AS WEBSOCKET, msg AS STRING)
    ' echo o comando dal browser — per ora logga
    PRINT "[canvas ws] msg: " + msg
    ' se il browser invia {"op":"demo"}, re-invia il frame
    IF INSTR(msg, "demo") > 0 THEN
        WEBCANVAS.Clear(ws, 0)
        WEBCANVAS.PSet(ws, 10, 10, 4)
        WEBCANVAS.Flush(ws)
    END IF
END SUB

SUB CanvasOnClose(ws AS WEBSOCKET)
    PRINT "[canvas ws] closed"
END SUB

