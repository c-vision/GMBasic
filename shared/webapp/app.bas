' FILE: webapp/app.bas — Launcher unificato WEBAPP GMBasic (C1b + Canvas + WASM)
' =============================================================================
' Avvia la WEBAPP Agents Dashboard (C1b, 5 tab) + Canvas WS + WASM shell.
' Tutto in BASIC: seed agenti/team demo, HTTP SERVER + BSP, proxy /api same-origin,
' WebSocket /ws/canvas con WEBCANVAS.
'
' Porte (editabili in Main):
'   API agents: 8765 (AI AGENT SERVE — /api/agents|teams|events|workflows|memory|brain|enrich)
'   Dashboard:  8090 (HTTP SERVER — serve pages/*.bsp renderizzate + static/*,
'                e fa da PROXY same-origin di TUTTO /api/* verso 8765)
'
' Avvio (dalla ROOT del monorepo):
'   dotnet GMBasicSharp/REPL/bin/Debug/net10.0/REPL.dll -noconsole shared/webapp/app.bas
'   poi apri http://localhost:8090/index.bsp  (o /canvas.bsp, /wasm.bsp)
'   API diretta: curl http://localhost:8765/api/agents
'   API via proxy: curl http://localhost:8090/api/agents
'
' Percorsi: pagine e statici sono risolti dalla CARTELLA DELLA APP (PROJECTFOLDER),
' non dalla working directory → l'app funziona da qualunque CWD.
'
' Editabilità: l'utente modifica qui porte, seed, route proxy, handler WS, senza toccare C#.
' Il file è il launcher di riferimento; i sample in samples/Community/AI/* sono copie minimali.
' =============================================================================

IMPORT HTTP
IMPORT JSON
IMPORT AI
IMPORT DASHBOARD
IMPORT WEBCANVAS

' Porta del server agenti (usata anche dal proxy /api/*)
DIM g_apiPort AS INTEGER

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
    g_apiPort = apiPort%
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

    ' Serve BSP e statici. Path ASSOLUTI calcolati dalla cartella della app: il server
    ' deve funzionare da qualunque working directory (prima erano "./webapp/pages",
    ' validi solo con CWD = shared/).
    DIM rootDir AS STRING
    DIM pagesDir AS STRING
    DIM staticDir AS STRING
    rootDir = FindAppRoot()
    pagesDir = rootDir + "/pages"
    staticDir = rootDir + "/static"
    IF NOT IO.DIREXISTS(pagesDir) THEN
        PRINT "errore: cartella pagine non trovata: "; pagesDir
        PRINT "  root=["; rootDir; "] PROJECTFOLDER=["; PROJECTFOLDER(); "] CWD=["; IO.CWD(); "]"
        ERROR 2
    END IF
    HTTP STATIC "/", pagesDir
    HTTP STATIC "/static", staticDir

    ' --- 3. Proxy same-origin /api/* → 8765 (evita CORS per JS fetch("/api/agents")) ---
    RegisterApiRoutes()

    ' --- 4. WebSocket Canvas (Vision-Ext §1) ---
    HTTP ROUTE "GET", "/ws/canvas", CanvasWsHandler()

    PRINT "pagine    : "; pagesDir
    PRINT "statici   : "; staticDir
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

' --- Percorsi ---------------------------------------------------------------

FUNCTION RTrimSlash(p AS STRING) AS STRING
    DIM s AS STRING
    s = p
    IF LEN(s) > 1 THEN
        IF RIGHT$(s, 1) = "/" THEN s = LEFT$(s, LEN(s) - 1)
    END IF
    RTrimSlash = s
END FUNCTION

' Cartella della app (contiene pages/ e static/). Accetta sia il layout attuale
' (app.bas accanto a pages/) sia quello con un livello src/ (app.bas in src/).
FUNCTION FindAppRoot() AS STRING
    DIM p AS STRING
    p = PROJECTFOLDER()
    IF LEN(p) = 0 THEN p = IO.CWD()
    p = RTrimSlash(p)
    IF IO.DIREXISTS(p + "/pages") THEN
        FindAppRoot = p
    ELSEIF IO.DIREXISTS(IO.PARENTDIR(p) + "/pages") THEN
        FindAppRoot = IO.PARENTDIR(p)
    ELSE
        FindAppRoot = p
    END IF
END FUNCTION

' --- Proxy handlers (BASIC, editabili) -------------------------------------
' Tutta la superficie /api/* è inoltrata a `AI AGENT SERVE` (127.0.0.1:8765):
' il server agenti implementa agents/teams/events/workflow(+step/execute/delete)/
' team broadcast/memory/brain/enrich. Un solo helper per metodo, il path viaggia
' così com'è (compresi i segmenti dinamici di HTTP ROUTE).

SUB RegisterApiRoutes()
    ' GET
    HTTP ROUTE "GET", "/api/agents", PxGet()
    HTTP ROUTE "GET", "/api/teams", PxGet()
    HTTP ROUTE "GET", "/api/teams/{name}", PxGet()
    HTTP ROUTE "GET", "/api/events", PxGet()
    HTTP ROUTE "GET", "/api/workflows", PxGet()
    HTTP ROUTE "GET", "/api/workflows/{name}", PxGet()
    HTTP ROUTE "GET", "/api/memory", PxGet()
    HTTP ROUTE "GET", "/api/memory/search", PxGet()
    HTTP ROUTE "GET", "/api/brain/search", PxGet()
    HTTP ROUTE "GET", "/api/brain/list", PxGet()
    ' POST
    HTTP ROUTE "POST", "/api/agent/{name}/prompt", PxPost()
    HTTP ROUTE "POST", "/api/workflows", PxPost()
    HTTP ROUTE "POST", "/api/workflows/{name}/steps", PxPost()
    HTTP ROUTE "POST", "/api/workflows/{name}/execute", PxPost()
    HTTP ROUTE "POST", "/api/team/{name}/broadcast", PxPost()
    HTTP ROUTE "POST", "/api/memory", PxPost()
    HTTP ROUTE "POST", "/api/enrich", PxPost()
    HTTP ROUTE "POST", "/api/brain/note", PxPost()
    HTTP ROUTE "POST", "/api/brain/save", PxPost()
    HTTP ROUTE "POST", "/api/brain/forget", PxPost()
    HTTP ROUTE "POST", "/api/brain/import", PxPost()
    HTTP ROUTE "POST", "/api/brain/watch", PxPost()
    HTTP ROUTE "POST", "/api/brain/refresh", PxPost()
    HTTP ROUTE "POST", "/api/brain/consolidate", PxPost()
    ' DELETE
    HTTP ROUTE "DELETE", "/api/memory", PxDelete()
    HTTP ROUTE "DELETE", "/api/workflows/{name}", PxDelete()
END SUB

SUB PxGet(req AS HTTPREQUEST, res AS HTTPRESPONSE)
    res.Send(Forward(req, "GET", ""))
END SUB

SUB PxPost(req AS HTTPREQUEST, res AS HTTPRESPONSE)
    res.Send(Forward(req, "POST", req.Body))
END SUB

SUB PxDelete(req AS HTTPREQUEST, res AS HTTPRESPONSE)
    res.Send(Forward(req, "DELETE", ""))
END SUB

' Inoltra la richiesta al server agenti mantenendo path e query string; il corpo
' è quello del client (già JSON). Risposta = corpo della risposta, così il JS fa
' `r.json()` senza CORS. Se il server agenti non è raggiungibile si restituisce
' un JSON esplicito (mai un errore HTML che farebbe fallire il parsing).
FUNCTION Forward(req AS HTTPREQUEST, method AS STRING, body AS STRING) AS STRING
    DIM url AS STRING
    DIM opts AS HTTPOPTIONS = NEW HTTPOPTIONS
    DIM r AS FETCHRESULT
    url = "http://127.0.0.1:" + LTRIM$(STR$(g_apiPort)) + req.Path
    IF LEN(req.QueryString) > 0 THEN url = url + "?" + req.QueryString
    opts.Method = method
    IF LEN(body) > 0 THEN opts.Body = body
    opts.Headers.Add("Content-Type", "application/json")
    r = FETCH(url, opts)
    IF r.StatusCode = 0 THEN
        ' JSON costruito con CHR$(34): la forma con "" ripetuti dopo un + è ambigua
        ' per il lexer (`"""` viene letta come stringa vuota + inizio stringa).
        DIM q AS STRING
        q = CHR$(34)
        Forward = "{" + q + "error" + q + ":" + q + "agent-api-unreachable" + q + "," + q + "url" + q + ":" + q + EscapeJson(url) + q + "}"
    ELSE
        Forward = r.Body
    END IF
END FUNCTION

FUNCTION EscapeJson(s AS STRING) AS STRING
    DIM t AS STRING
    t = s
    t = REPLACE$(t, "\", "\\")
    t = REPLACE$(t, CHR$(34), "\" + CHR$(34))
    t = REPLACE$(t, CHR$(10), "\n")
    t = REPLACE$(t, CHR$(13), "")
    t = REPLACE$(t, CHR$(9), " ")
    EscapeJson = t
END FUNCTION

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

