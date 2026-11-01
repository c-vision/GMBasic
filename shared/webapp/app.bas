' FILE: shared/webapp/app.bas — WEBAPP "Agents Operating Center" (tutto in BASIC)
' ============================================================================
' Cruscotto web dello stack AI Agents di GMBasic. Nulla è cablato in C#: questo
' file avvia i server, espone i dati e orchestra il frontend BSP.
'
'   ┌──────────────────────────── 8090 ─────────────────────────────┐
'   │ pages/*.bsp (SSR)  ·  static/*  ·  proxy /api/*  ·  WS canvas │
'   └───────────────────────────────┬───────────────────────────────┘
'                                   │ FETCH (same-origin proxy, no CORS)
'   ┌──────────────────────────── 8765 ─────────────────────────────┐
'   │ AI AGENT SERVE: agents · teams · events · workflows · memory   │
'   │                 brain · enrich · agent prompt                  │
'   └───────────────────────────────────────────────────────────────┘
'
' Avvio (dalla root del monorepo, in-process PCode — nessun host esterno):
'   dotnet GMBasicSharp/REPL/bin/Debug/net10.0/REPL.dll -noconsole --pcode shared/webapp/app.bas
' oppure, con gestione start/stop/log:
'   tools/sh/webapp_up.sh start
'
' Configurazione: .webapp.toml accanto a questo file (porte, seed, refresh,
' restrizione di rete, percorsi). Nessun parametro hardcoded.
'
' Endpoint propri (oltre al proxy completo di /api/*):
'   GET    /api/health               → {"ok":true}
'   GET    /api/status               → stato app/runtime (provider, conteggi, uptime)
'   POST   /api/agents               → crea agente {name, systemPrompt?, permission?,
'                                       temperature?, model?}   (409 se esiste)
'   DELETE /api/agents/{name}        → elimina agente
'   POST   /api/agents/{name}/ask    → {prompt} → risposta SINCRONA (AI.AGENTASKSYNC)
'   GET    /api/agents|teams|events|workflows|memory|brain/*|enrich  → proxy al 8765
'   POST/DELETE idem (proxy trasparente, path e query invariati)
'   GET    /ws/canvas                → WebSocket con i frame di WEBCANVAS
' ============================================================================

IMPORT HTTP
IMPORT JSON
IMPORT STRINGS
IMPORT IO
IMPORT DATETIME
IMPORT AI
IMPORT DASHBOARD
IMPORT WEBCANVAS

' --- Configurazione (default; .webapp.toml li sovrascrive) ------------------
DIM g_version AS STRING
DIM g_dashPort AS INTEGER
DIM g_apiPort AS INTEGER
DIM g_allowedIPs AS STRING
DIM g_sessions AS INTEGER
DIM g_seedDemo AS INTEGER
DIM g_autoTrace AS INTEGER
DIM g_refreshMs AS INTEGER
DIM g_rateLimitPerMin AS INTEGER
DIM g_rateLimitBurst AS INTEGER
DIM g_bspCacheMs AS INTEGER
DIM g_bspEngine AS STRING
DIM g_bspEnginePages AS STRING
DIM g_bspConcurrentPages AS STRING

' --- Stato del processo -----------------------------------------------------
DIM g_appDir AS STRING
DIM g_cfgPath AS STRING
DIM g_pagesDir AS STRING
DIM g_staticDir AS STRING
DIM g_agentsDir AS STRING
DIM g_teamsDir AS STRING
DIM g_startSec AS DOUBLE
DIM g_startTime AS STRING
DIM g_proxied AS LONG

' ============================================================================
SUB Main()
    SetDefaults()
    g_appDir = FindAppRoot()
    LoadConfig()
    ResolvePaths()
    PrepareData()
    PrintBanner()

    ' Persistenza (bin/agents, bin/teams): carica PRIMA di decidere il seed.
    LoadPersistedAgents()
    SeedDemo()
    IF g_autoTrace = 1 THEN
        AI AGENT TRACE VERBOSE
        PRINT "trace      : VERBOSE (events in the Log)"
    END IF

    StartAgentApi()
    LoadPersistedTeams()
    StartDashboard()

    g_startSec = DATETIME.SECONDS()
    g_startTime = STRTIME
    PRINT ""
    PRINT "ready. CTRL+C to stop, or: tools/sh/webapp_up.sh stop"
    PRINT ""

    SLEEP 3600000

    HTTP SERVER STOP
    AI AGENT STOP SERVER
    PRINT "servers stopped."
END SUB

' ============================================================================
' Configurazione
' ============================================================================
SUB SetDefaults()
    g_version = "2.0"
    g_dashPort = 8090
    g_apiPort = 8765
    g_allowedIPs = "127.0.0.1"
    g_sessions = 1
    g_seedDemo = 1
    g_autoTrace = 1
    g_refreshMs = 2500
    g_rateLimitPerMin = 0
    g_rateLimitBurst = 0
    g_bspCacheMs = 0
    g_bspEngine = "c"
    g_bspEnginePages = ""
    g_bspConcurrentPages = ""
    g_proxied = 0
END SUB

' Cerca `.webapp.toml` nella cartella della app e risalendo (max 4 livelli).
SUB LoadConfig()
    DIM dir AS STRING
    DIM cand AS STRING
    DIM found AS INTEGER
    DIM i AS INTEGER
    dir = g_appDir
    found = 0
    FOR i = 1 TO 4
        cand = RTrimSlash(dir) + "/.webapp.toml"
        IF found = 0 AND IO.FILEEXISTS(cand) THEN
            g_cfgPath = cand
            ParseConfig(cand)
            found = 1
        END IF
        dir = IO.PARENTDIR(dir)
        IF LEN(dir) = 0 THEN i = 4
    NEXT i
END SUB

' Parser TOML minimale: [sezione], chiave = valore, commenti '#', stringhe
' quotate, interi, booleani. Stesso sottoinsieme delle altre app del monorepo.
SUB ParseConfig(path AS STRING)
    DIM lines
    DIM raw AS STRING, section AS STRING, key AS STRING, val AS STRING
    DIM eq AS INTEGER
    DIM i AS LONG
    lines = IO.READLINES(path)
    section = ""
    FOR i = 0 TO UBOUND(lines)
        raw = STRINGS.TRIM(lines(i))
        IF LEN(raw) > 0 AND NOT STRINGS.STARTSWITH(raw, "#") THEN
            IF STRINGS.STARTSWITH(raw, "[") THEN
                section = STRINGS.UPPER(BracketInner(raw))
            ELSE
                eq = INSTR(raw, "=")
                IF eq > 1 THEN
                    key = STRINGS.UPPER(STRINGS.TRIM(LEFT$(raw, eq - 1)))
                    val = Unquote(STRINGS.TRIM(MID$(raw, eq + 1, LEN(raw) - eq)))
                    IF section = "SERVER" THEN
                        IF key = "DASHBOARDPORT" THEN g_dashPort = CINT(VAL(val))
                        IF key = "APIPORT" THEN g_apiPort = CINT(VAL(val))
                        IF key = "ALLOWEDIPS" THEN g_allowedIPs = val
                        IF key = "SESSIONS" THEN g_sessions = BoolVal(val)
                        IF key = "RATELIMITPERMINUTE" THEN g_rateLimitPerMin = CINT(VAL(val))
                        IF key = "RATELIMITBURST" THEN g_rateLimitBurst = CINT(VAL(val))
                        IF key = "BSPCACHEMS" THEN g_bspCacheMs = CINT(VAL(val))
                        IF key = "BSPENGINE" THEN g_bspEngine = STRINGS.LOWER(val)
                        IF key = "BSPENGINEPAGES" THEN g_bspEnginePages = val
                        IF key = "BSPCONCURRENTPAGES" THEN g_bspConcurrentPages = val
                    ELSEIF section = "APP" THEN
                        IF key = "SEEDDEMO" THEN g_seedDemo = BoolVal(val)
                        IF key = "AUTOTRACE" THEN g_autoTrace = BoolVal(val)
                        IF key = "REFRESHMS" THEN g_refreshMs = CINT(VAL(val))
                    END IF
                END IF
            END IF
        END IF
    NEXT i
END SUB

FUNCTION BracketInner(s AS STRING) AS STRING
    BracketInner = STRINGS.TRIM(MID$(s, 2, LEN(s) - 2))
END FUNCTION

FUNCTION BoolVal(s AS STRING) AS INTEGER
    DIM t AS STRING
    t = STRINGS.UPPER(STRINGS.TRIM(s))
    IF t = "1" OR t = "TRUE" OR t = "ON" OR t = "YES" THEN
        BoolVal = 1
    ELSE
        BoolVal = 0
    END IF
END FUNCTION

' Rimuove le virgolette di una stringa TOML e i commenti finali fuori stringa.
FUNCTION Unquote(s AS STRING) AS STRING
    DIM t AS STRING
    t = s
    IF LEN(t) >= 2 THEN
        IF LEFT$(t, 1) = CHR$(34) THEN
            DIM closeAt AS INTEGER
            closeAt = INSTR(MID$(t, 2, LEN(t) - 1), CHR$(34))
            IF closeAt > 0 THEN t = MID$(t, 2, closeAt - 1)
        END IF
    END IF
    Unquote = STRINGS.TRIM(t)
END FUNCTION

' ============================================================================
' Percorsi (sempre relativi alla CARTELLA DELLA APP, mai alla CWD)
' ============================================================================
FUNCTION RTrimSlash(p AS STRING) AS STRING
    DIM s AS STRING
    s = p
    IF LEN(s) > 1 THEN
        IF RIGHT$(s, 1) = "/" THEN s = LEFT$(s, LEN(s) - 1)
    END IF
    RTrimSlash = s
END FUNCTION

FUNCTION FindAppRoot() AS STRING
    DIM p AS STRING
    p = PROJECTFOLDER()
    IF LEN(p) = 0 THEN p = IO.CWD()
    IF LEN(p) = 0 THEN p = "."
    p = RTrimSlash(p)
    IF IO.DIREXISTS(p + "/pages") THEN
        FindAppRoot = p
    ELSEIF IO.DIREXISTS(IO.PARENTDIR(p) + "/pages") THEN
        FindAppRoot = IO.PARENTDIR(p)
    ELSEIF IO.DIREXISTS("./pages") THEN
        FindAppRoot = "."
    ELSE
        FindAppRoot = p
    END IF
END FUNCTION

SUB ResolvePaths()
    g_pagesDir = RelativeTo(g_appDir, "pages")
    g_staticDir = RelativeTo(g_appDir, "static")
END SUB

FUNCTION RelativeTo(rootDir AS STRING, child AS STRING) AS STRING
    ' NB: niente parametri chiamati `base`/`sub`: sono KEYWORD del linguaggio e il
    ' legame dei nomi si rompe (risultato: path vuoto → "/pages").
    IF STRINGS.STARTSWITH(child, "/") THEN
        RelativeTo = child
    ELSE
        RelativeTo = RTrimSlash(rootDir) + "/" + child
    END IF
END FUNCTION

' Directory dei dati (artefatti della app: mai nel sorgente, sempre in bin/).
SUB PrepareData()
    DIM binDir AS STRING
    binDir = RTrimSlash(g_appDir) + "/bin"
    g_agentsDir = binDir + "/agents"
    g_teamsDir = binDir + "/teams"
    IF NOT IO.DIREXISTS(binDir) THEN MKDIR (binDir)
    IF NOT IO.DIREXISTS(g_agentsDir) THEN MKDIR (g_agentsDir)
    IF NOT IO.DIREXISTS(g_teamsDir) THEN MKDIR (g_teamsDir)
END SUB

FUNCTION AgentPath(name AS STRING) AS STRING
    AgentPath = g_agentsDir + "/" + name + ".agent.json"
END FUNCTION

FUNCTION TeamPath(name AS STRING) AS STRING
    TeamPath = g_teamsDir + "/" + name + ".team.json"
END FUNCTION

FUNCTION FileStem(f AS STRING, suffix AS STRING) AS STRING
    DIM s AS STRING
    s = f
    IF STRINGS.ENDSWITH(STRINGS.UPPER(s), STRINGS.UPPER(suffix)) THEN
        s = LEFT$(s, LEN(s) - LEN(suffix))
    END IF
    FileStem = s
END FUNCTION

' Ricarica dal disco gli agenti salvati (AI AGENT SAVE produce <nome>.agent.json).
SUB LoadPersistedAgents()
    DIM fs() = IO.DIR(g_agentsDir + "/*.agent.json")
    DIM i AS INTEGER
    IF UBOUND(fs) < 0 THEN
        PRINT "agents     : none on disk ("; g_agentsDir; ")"
        EXIT SUB
    END IF
    FOR i = 0 TO UBOUND(fs)
        DIM fn AS STRING
        DIM nm AS STRING
        fn = fs(i)
        nm = FileStem(fn, ".agent.json")
        AI AGENT LOAD (nm), (g_agentsDir + "/" + fn)
    NEXT i
    PRINT "agents     : "; UBOUND(fs) + 1; " loaded from bin/agents"
END SUB

SUB SaveAgent(name AS STRING)
    AI AGENT SAVE (name), (AgentPath(name))
END SUB

SUB SavePersistedAgent(name AS STRING)
    IF LEN(name) = 0 THEN EXIT SUB
    AI AGENT SAVE (name), (AgentPath(name))
END SUB

SUB DeletePersistedAgent(name AS STRING)
    DIM p AS STRING
    p = AgentPath(name)
    IF IO.FILEEXISTS(p) THEN KILL (p)
END SUB

' I team si caricano via API agenti (l'API parte prima di questa chiamata).
SUB LoadPersistedTeams()
    DIM fs() = IO.DIR(g_teamsDir + "/*.team.json")
    DIM i AS INTEGER
    DIM ok AS INTEGER
    IF UBOUND(fs) < 0 THEN EXIT SUB
    ok = 0
    FOR i = 0 TO UBOUND(fs)
        DIM fn AS STRING
        DIM nm AS STRING
        DIM body AS STRING
        DIM r AS STRING
        fn = fs(i)
        nm = FileStem(fn, ".team.json")
        body = "{" + JQ("name") + ":" + JStr(nm) + "," + JQ("path") + ":" + JStr(g_teamsDir + "/" + fn) + "}"
        r = ApiCall("POST", "/api/teams/load", body)
        IF INSTR(r, "loaded") > 0 THEN ok = ok + 1
    NEXT i
    PRINT "teams      : "; ok; " loaded from bin/teams"
END SUB

SUB SavePersistedTeam(name AS STRING)
    DIM p AS STRING
    DIM body AS STRING
    IF LEN(name) = 0 THEN EXIT SUB
    p = TeamPath(name)
    body = "{" + JQ("path") + ":" + JStr(p) + "}"
    DIM r AS STRING
    r = ApiCall("POST", "/api/teams/" + name + "/save", body)
END SUB

SUB DeletePersistedTeam(name AS STRING)
    DIM p AS STRING
    p = TeamPath(name)
    IF IO.FILEEXISTS(p) THEN KILL (p)
END SUB

SUB PrintBanner()
    DIM cfg AS STRING
    cfg = "(default interni)"
    IF LEN(g_cfgPath) > 0 THEN cfg = g_cfgPath
    PRINT "┌────────────────────────────────────────────────────────────┐"
    PRINT "│  GMBasic WEBAPP — Agents Operating Center  v"; g_version; "             │"
    PRINT "└────────────────────────────────────────────────────────────┘"
    PRINT "config     : "; cfg
    PRINT "app dir    : "; g_appDir
    PRINT "pages      : "; g_pagesDir
    PRINT "static     : "; g_staticDir
    PRINT "network    : "; AllowedText()
    PRINT "refresh    : "; g_refreshMs; " ms"
    IF g_rateLimitPerMin > 0 THEN
        PRINT "rate limit : "; g_rateLimitPerMin; "/min (burst "; g_rateLimitBurst; ")"
    ELSE
        PRINT "rate limit : none"
    END IF
    IF g_bspCacheMs > 0 THEN
        PRINT "bsp cache  : "; g_bspCacheMs; " ms (ETag/304 + gzip)"
    ELSE
        PRINT "bsp cache  : off (ETag/304 + gzip sempre attivi)"
    END IF
    PRINT "bsp engine : "; g_bspEngine
    IF LEN(g_bspEnginePages) > 0 THEN PRINT "bsp c pages: "; g_bspEnginePages
    IF LEN(g_bspConcurrentPages) > 0 THEN PRINT "bsp conc   : "; g_bspConcurrentPages
END SUB

FUNCTION ApiUrl() AS STRING
    ApiUrl = "http://127.0.0.1:" + LTRIM$(STR$(g_apiPort)) + "/api/agents"
END FUNCTION

FUNCTION AllowedText() AS STRING
    IF LEN(g_allowedIPs) = 0 THEN
        AllowedText = "no filter (all interfaces)"
    ELSE
        AllowedText = "loopback only: " + g_allowedIPs
    END IF
END FUNCTION

' ============================================================================
' Seed demo (solo se il registry è vuoto e seedDemo = 1)
' ============================================================================
SUB SeedDemo()
    DIM names() = AI.AGENTLIST()
    IF UBOUND(names) >= 0 THEN
        EXIT SUB   ' già caricati (il messaggio l'ha dato LoadPersistedAgents)
    END IF
    IF g_seedDemo = 0 THEN
        PRINT "agents     : 0 (seed disabled in .webapp.toml)"
        EXIT SUB
    END IF
    PRINT "agents     : 0 → creating Demo, Analyst, Support"
    AI AGENT CREATE "Demo"
    AI AGENT SET "Demo" systemPrompt "You are a helpful demo agent."
    AI AGENT SET "Demo" permission YOLO
    AI AGENT CREATE "Analyst"
    AI AGENT SET "Analyst" systemPrompt "You are a concise analyst."
    AI AGENT SET "Analyst" temperature 0.3
    AI AGENT CREATE "Support"
    AI AGENT SET "Support" systemPrompt "You are support."
    ' Persisti subito il set dimostrativo: dal prossimo avvio si ricarica dal disco
    ' (senza, il seed veniva ricreato a ogni run e non sopravviveva ai riavvii).
    SavePersistedAgent("Demo")
    SavePersistedAgent("Analyst")
    SavePersistedAgent("Support")
END SUB

' ============================================================================
' Server
' ============================================================================
SUB StartAgentApi()
    DIM r AS FETCHRESULT
    IF g_apiPort <= 0 THEN
        PRINT "agent api  : disabled (apiPort = 0 in .webapp.toml)"
        EXIT SUB
    END IF
    ' Porta dinamica: la parentesi evita che il parser la prenda per un NOME
    ' (AI AGENT SERVE accetta espressioni, ma un identificatore nudo diventa stringa).
    AI AGENT SERVE (g_apiPort)
    SLEEP 300
    r = FETCH("http://127.0.0.1:" + LTRIM$(STR$(g_apiPort)) + "/api/agents")
    IF r.StatusCode = 200 THEN
        PRINT "agent api  : " + ApiUrl()
    ELSE
        PRINT "agent api  : STARTED but not answering on :" + LTRIM$(STR$(g_apiPort)) + " (port busy? see .webapp.toml)"
    END IF
END SUB

SUB StartDashboard()
    DIM cfg AS HTTPCONFIG = NEW HTTPCONFIG
    IF g_dashPort <= 0 THEN
        PRINT "dashboard  : invalid port ("; g_dashPort; ")"
        EXIT SUB
    END IF
    IF NOT IO.DIREXISTS(g_pagesDir) THEN
        PRINT "error: pages folder not found: "; g_pagesDir
        ERROR 2
    END IF
    cfg.Port = g_dashPort
    IF g_sessions = 1 THEN cfg.Sessions = TRUE
    IF LEN(g_allowedIPs) > 0 THEN cfg.AllowedIPs = g_allowedIPs
    ' Rate limit: il DEFAULT del runtime è 60/min (burst 120) — una console locale con
    ' polling lo esaurisce e riceve 429. Qui si parte da 0 (nessun limite) e si può
    ' alzare da .webapp.toml per un'esposizione in LAN.
    cfg.RateLimitRequestsPerMinute = g_rateLimitPerMin
    cfg.RateLimitBurstSize = g_rateLimitBurst
    ' Memoizzazione della resa BSP: riusa il corpo per N ms (ETag/304 inclusi). 0 = sempre
    ' eseguita. Vedi .webapp.toml.
    cfg.BspCacheMs = g_bspCacheMs
    cfg.BspEngine = g_bspEngine
    cfg.BspEnginePages = g_bspEnginePages
    cfg.BspConcurrentPages = g_bspConcurrentPages
    HTTP CONFIG cfg

    ' Più mount statici sono legittimi: vince il prefisso più lungo.
    HTTP STATIC "/", g_pagesDir
    HTTP STATIC "/static", g_staticDir

    RegisterApiRoutes()
    HTTP ROUTE "GET", "/ws/canvas", CanvasWsHandler()

    HTTP SERVER cfg.Port
    PRINT "dashboard  : http://127.0.0.1:" + LTRIM$(STR$(g_dashPort)) + "/index.bsp"
END SUB

' ============================================================================
' Rotte: endpoint PROPRI (BASIC) + proxy trasparente verso l'API agenti
' ============================================================================
SUB RegisterApiRoutes()
    ' --- endpoint implementati qui in BASIC ---
    HTTP ROUTE "GET", "/api/health", ApiHealth()
    HTTP ROUTE "GET", "/api/status", ApiStatus()
    HTTP ROUTE "POST", "/api/agents", ApiAgentCreate()
    HTTP ROUTE "PATCH", "/api/agents/{name}", ApiAgentUpdate()
    HTTP ROUTE "DELETE", "/api/agents/{name}", ApiAgentDelete()
    HTTP ROUTE "POST", "/api/agents/{name}/ask", ApiAgentAsk()
    HTTP ROUTE "POST", "/api/teams", ApiTeamCreate()
    HTTP ROUTE "POST", "/api/teams/{name}/agents", ApiTeamAddAgent()
    HTTP ROUTE "DELETE", "/api/teams/{name}", ApiTeamDelete()

    ' --- proxy GET ---
    HTTP ROUTE "GET", "/api/agents", PxGet()
    HTTP ROUTE "GET", "/api/teams", PxGet()
    HTTP ROUTE "GET", "/api/teams/{name}", PxGet()
    HTTP ROUTE "GET", "/api/events", PxGet()
    HTTP ROUTE "GET", "/api/events/stream", ApiEventStream()
    HTTP ROUTE "GET", "/api/workflows", PxGet()
    HTTP ROUTE "GET", "/api/workflows/{name}", PxGet()
    HTTP ROUTE "GET", "/api/memory", PxGet()
    HTTP ROUTE "GET", "/api/memory/search", PxGet()
    HTTP ROUTE "GET", "/api/brain/search", PxGet()
    HTTP ROUTE "GET", "/api/brain/list", PxGet()

    ' --- proxy POST ---
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

    ' --- proxy DELETE ---
    HTTP ROUTE "DELETE", "/api/memory", PxDelete()
    HTTP ROUTE "DELETE", "/api/workflows/{name}", PxDelete()
END SUB

' --- Endpoint propri -------------------------------------------------------

SUB ApiHealth(req AS HTTPREQUEST, res AS HTTPRESPONSE)
    res.Send("{" + JQ("ok") + ":true," + JQ("app") + ":" + JStr("webapp") + "}")
END SUB

' Stato della app + del runtime: la UI lo interroga per le "chip" in testa e per
' il polling. Tutto costruito qui, nessuna dipendenza dal server agenti tranne
' l'eventuale reachability (una GET leggera).
' ---------------------------------------------------------------------------
' GET /api/events/stream — SSE (text/event-stream, chunked): manda gli eventi
' NUOVI del log agente appena compaiono, piu' un commento di keep-alive ogni
' mezzo secondo. La dashboard lo consuma con EventSource (fallback: polling
' su /api/events, che resta invariato).
' ---------------------------------------------------------------------------
SUB ApiEventStream(req AS HTTPREQUEST, res AS HTTPRESPONSE)
    DIM sent AS LONG
    DIM i AS LONG
    DIM k AS LONG
    DIM n AS LONG
    DIM evs() AS STRING

    res.StartStream("text/event-stream")
    sent = 0
    ' ~5 minuti di stream per connessione, poi il client si riconnette.
    FOR i = 1 TO 600
        evs = AI.AGENTEVENTS()
        n = UBOUND(evs) + 1
        IF n > sent THEN
            FOR k = sent TO n - 1
                ' JSON.STRINGIFY produce una stringa JSON con gli escape: il payload SSE
                ' non puo' contenere newline grezzi.
                res.Write("data: " + JSON.STRINGIFY(evs(k)) + CHR$(10) + CHR$(10))
            NEXT k
            sent = n
            res.Flush()
        ELSE
            ' keep-alive (commento SSE) ogni ~2 s, senza svegliare il client
            IF (i MOD 4) = 0 THEN
                res.Write(": ping" + CHR$(10) + CHR$(10))
                res.Flush()
            END IF
        END IF
        SLEEP 500
    NEXT i
END SUB

SUB ApiStatus(req AS HTTPREQUEST, res AS HTTPRESPONSE)
    DIM up AS LONG
    DIM nAgents AS INTEGER
    DIM nEvents AS INTEGER
    DIM active AS STRING
    DIM model AS STRING
    DIM reach AS INTEGER
    DIM sb AS STRING
    DIM r AS FETCHRESULT

    up = CLNG(DATETIME.SECONDS() - g_startSec)
    IF g_startSec = 0 THEN up = 0
    IF up < 0 THEN up = up + 86400

    DIM names() = AI.AGENTLIST()
    nAgents = UBOUND(names) + 1
    DIM tlist AS STRING
    DIM nTeams AS INTEGER
    tlist = AI.TEAMLIST()
    IF LEN(tlist) = 0 THEN
        nTeams = 0
    ELSE
        DIM ts() AS STRING = SPLIT(tlist, CHR$(10))
        nTeams = UBOUND(ts) + 1
    END IF
    DIM evs() = AI.AGENTEVENTS()
    nEvents = UBOUND(evs) + 1
    active = AI.ACTIVE(%CHAT)
    model = ""
    IF LEN(active) > 0 THEN model = AI.GETMODEL(active)

    reach = 0
    IF g_apiPort > 0 THEN
        r = FETCH("http://127.0.0.1:" + LTRIM$(STR$(g_apiPort)) + "/api/agents")
        IF r.StatusCode = 200 THEN reach = 1
    END IF

    sb = "{"
    sb = sb + JQ("app") + ":" + JStr("webapp")
    sb = sb + "," + JQ("version") + ":" + JStr(g_version)
    sb = sb + "," + JQ("dashboardPort") + ":" + LTRIM$(STR$(g_dashPort))
    sb = sb + "," + JQ("apiPort") + ":" + LTRIM$(STR$(g_apiPort))
    sb = sb + "," + JQ("apiReachable") + ":" + BoolJson(reach)
    sb = sb + "," + JQ("agents") + ":" + LTRIM$(STR$(nAgents))
    sb = sb + "," + JQ("events") + ":" + LTRIM$(STR$(nEvents))
    sb = sb + "," + JQ("teams") + ":" + LTRIM$(STR$(nTeams))
    sb = sb + "," + JQ("dataDir") + ":" + JStr(g_agentsDir)
    sb = sb + "," + JQ("activeProvider") + ":" + JStr(active)
    sb = sb + "," + JQ("model") + ":" + JStr(model)
    sb = sb + "," + JQ("refreshMs") + ":" + LTRIM$(STR$(g_refreshMs))
    sb = sb + "," + JQ("startedAt") + ":" + JStr(g_startTime)
    sb = sb + "," + JQ("uptimeSec") + ":" + LTRIM$(STR$(up))
    sb = sb + "," + JQ("proxied") + ":" + LTRIM$(STR$(g_proxied))
    sb = sb + "," + JQ("pages") + ":" + JStr(g_pagesDir)
    sb = sb + "," + JQ("static") + ":" + JStr(g_staticDir)
    sb = sb + "," + JQ("allowedIPs") + ":" + JStr(g_allowedIPs)
    sb = sb + "}"
    g_proxied = g_proxied + 1
    res.Send(sb)
END SUB

' POST /api/agents {name, systemPrompt?, permission?, temperature?, model?}
SUB ApiAgentCreate(req AS HTTPREQUEST, res AS HTTPRESPONSE)
    DIM body AS STRING
    DIM name AS STRING
    DIM v AS STRING
    DIM names() AS STRING
    DIM i AS INTEGER
    DIM exists AS INTEGER

    body = req.Body
    name = JsonField(body, "name")
    IF LEN(STRINGS.TRIM(name)) = 0 THEN
        res.StatusCode = 400
        res.Send(ErrorJson("missing_name", "expected {""name"":""<agent>""}"))
        EXIT SUB
    END IF
    name = STRINGS.TRIM(name)

    exists = 0
    names = AI.AGENTLIST()
    FOR i = 0 TO UBOUND(names)
        IF STRINGS.UPPER(STRINGS.TRIM(names(i))) = STRINGS.UPPER(name) THEN exists = 1
    NEXT i
    IF exists = 1 THEN
        res.StatusCode = 409
        res.Send(ErrorJson("already_exists", "agent already exists: " + name))
        EXIT SUB
    END IF

    AI AGENT CREATE (name)
    v = JsonField(body, "systemPrompt")
    IF LEN(v) > 0 THEN AI AGENT SET (name) systemPrompt (v)
    v = JsonField(body, "permission")
    IF LEN(v) > 0 THEN AI AGENT SET (name) permission (v)
    v = JsonField(body, "temperature")
    IF LEN(v) > 0 THEN AI AGENT SET (name) temperature (VAL(v))
    v = JsonField(body, "model")
    IF LEN(v) > 0 THEN AI AGENT SET (name) model (v)

    SavePersistedAgent(name)
    PRINT "[api] agent created: "; name; " (from "; req.RemoteIp; ")"
    res.StatusCode = 201
    res.Send("{" + JQ("created") + ":" + JStr(name) + "}")
END SUB

' PATCH /api/agents/{name} {systemPrompt?, permission?, temperature?, model?}
' Aggiorna SOLO i campi presenti nel body (vuoto = lascia invariato).
SUB ApiAgentUpdate(req AS HTTPREQUEST, res AS HTTPRESPONSE)
    DIM name AS STRING
    DIM v AS STRING
    DIM changed AS INTEGER
    name = req.Params["name"]
    IF LEN(STRINGS.TRIM(name)) = 0 THEN
        res.StatusCode = 400
        res.Send(ErrorJson("missing_name", "missing agent name in path"))
        EXIT SUB
    END IF
    changed = 0
    v = JsonField(req.Body, "systemPrompt")
    IF LEN(v) > 0 THEN
        AI AGENT SET (name) systemPrompt (v)
        changed = changed + 1
    END IF
    v = JsonField(req.Body, "permission")
    IF LEN(v) > 0 THEN
        AI AGENT SET (name) permission (v)
        changed = changed + 1
    END IF
    v = JsonField(req.Body, "temperature")
    IF LEN(v) > 0 THEN
        AI AGENT SET (name) temperature (VAL(v))
        changed = changed + 1
    END IF
    v = JsonField(req.Body, "model")
    IF LEN(v) > 0 THEN
        AI AGENT SET (name) model (v)
        changed = changed + 1
    END IF
    IF changed > 0 THEN SavePersistedAgent(name)
    PRINT "[api] agent updated: "; name; " ("; changed; " fields)"
    res.Send("{" + JQ("updated") + ":" + JStr(name) + "," + JQ("fields") + ":" + LTRIM$(STR$(changed)) + "}")
END SUB

' DELETE /api/agents/{name}
SUB ApiAgentDelete(req AS HTTPREQUEST, res AS HTTPRESPONSE)
    DIM name AS STRING
    name = req.Params["name"]
    IF LEN(STRINGS.TRIM(name)) = 0 THEN
        res.StatusCode = 400
        res.Send(ErrorJson("missing_name", "missing agent name in path"))
        EXIT SUB
    END IF
    AI AGENT DELETE (name)
    DeletePersistedAgent(name)
    PRINT "[api] agent deleted: "; name; " (from "; req.RemoteIp; ")"
    res.Send("{" + JQ("deleted") + ":" + JStr(name) + "}")
END SUB

' POST /api/agents/{name}/ask {prompt} → risposta SINCRONA (nessun server di streaming)
SUB ApiAgentAsk(req AS HTTPREQUEST, res AS HTTPRESPONSE)
    DIM name AS STRING
    DIM prompt AS STRING
    DIM answer AS STRING
    name = req.Params["name"]
    prompt = JsonField(req.Body, "prompt")
    IF LEN(STRINGS.TRIM(prompt)) = 0 THEN
        res.StatusCode = 400
        res.Send(ErrorJson("missing_prompt", "expected {""prompt"":""<text>""}"))
        EXIT SUB
    END IF
    answer = AI.AGENTASKSYNC(name, prompt)
    res.Send("{" + JQ("agent") + ":" + JStr(name) + "," + JQ("answer") + ":" + JStr(answer) + "}")
END SUB

' --- Team (con persistenza su bin/teams) ------------------------------------
' Il runtime non accetta nomi dinamici nei comandi (`AI TEAM CREATE Nome` vuole un
' identificatore statico): la creazione/aggiunta passa dall'API agenti (endpoint V2)
' e l'app persiste il team nel SUO percorso (bin/teams/<nome>.team.json).

SUB ApiTeamCreate(req AS HTTPREQUEST, res AS HTTPRESPONSE)
    DIM name AS STRING
    DIM out AS STRING
    DIM errTag AS STRING
    name = STRINGS.TRIM(JsonField(req.Body, "name"))
    IF LEN(name) = 0 THEN
        res.StatusCode = 400
        res.Send(ErrorJson("missing_name", "expected {""name"":""<team>""}"))
        EXIT SUB
    END IF
    errTag = CHR$(34) + "error" + CHR$(34)
    out = ApiCall("POST", "/api/teams", req.Body)
    IF INSTR(out, errTag) > 0 THEN
        res.StatusCode = 409
        res.Send(out)
        EXIT SUB
    END IF
    SavePersistedTeam(name)
    PRINT "[api] team created: "; name
    res.StatusCode = 201
    res.Send(out)
END SUB

SUB ApiTeamAddAgent(req AS HTTPREQUEST, res AS HTTPRESPONSE)
    DIM name AS STRING
    DIM out AS STRING
    DIM errTag AS STRING
    name = STRINGS.TRIM(req.Params["name"])
    errTag = CHR$(34) + "error" + CHR$(34)
    IF LEN(name) = 0 THEN
        res.StatusCode = 400
        res.Send(ErrorJson("missing_name", "missing team name in path"))
        EXIT SUB
    END IF
    out = ApiCall("POST", "/api/teams/" + name + "/agents", req.Body)
    IF INSTR(out, errTag) > 0 THEN
        res.StatusCode = 404
        res.Send(out)
        EXIT SUB
    END IF
    SavePersistedTeam(name)
    PRINT "[api] team "; name; ": member added"
    res.Send(out)
END SUB

SUB ApiTeamDelete(req AS HTTPREQUEST, res AS HTTPRESPONSE)
    DIM name AS STRING
    DIM out AS STRING
    DIM errTag AS STRING
    name = STRINGS.TRIM(req.Params["name"])
    errTag = CHR$(34) + "error" + CHR$(34)
    IF LEN(name) = 0 THEN
        res.StatusCode = 400
        res.Send(ErrorJson("missing_name", "missing team name in path"))
        EXIT SUB
    END IF
    out = ApiCall("DELETE", "/api/teams/" + name, "")
    IF INSTR(out, errTag) > 0 THEN
        res.StatusCode = 404
        res.Send(out)
        EXIT SUB
    END IF
    DeletePersistedTeam(name)
    PRINT "[api] team deleted: "; name
    res.Send(out)
END SUB

' --- Proxy trasparente -----------------------------------------------------

SUB PxGet(req AS HTTPREQUEST, res AS HTTPRESPONSE)
    res.Send(Forward(req, "GET", ""))
END SUB

SUB PxPost(req AS HTTPREQUEST, res AS HTTPRESPONSE)
    res.Send(Forward(req, "POST", req.Body))
END SUB

SUB PxDelete(req AS HTTPREQUEST, res AS HTTPRESPONSE)
    res.Send(Forward(req, "DELETE", ""))
END SUB

' Inoltra metodo+path+query (+body) al server agenti; la risposta è il corpo
' JSON così il JS fa `r.json()` senza CORS. Se l'API è giù si restituisce un
' JSON esplicito (mai HTML), così la UI mostra un errore leggibile.
FUNCTION Forward(req AS HTTPREQUEST, method AS STRING, body AS STRING) AS STRING
    DIM p AS STRING
    p = req.Path
    IF LEN(req.QueryString) > 0 THEN p = p + "?" + req.QueryString
    Forward = ApiCall(method, p, body)
END FUNCTION

' Chiamata all'API agenti (path + eventuale query). Usata dal proxy e dagli
' endpoint propri che devono parlare col runtime (team, persistenza).
FUNCTION ApiCall(method AS STRING, pathQuery AS STRING, body AS STRING) AS STRING
    DIM url AS STRING
    DIM opts AS HTTPOPTIONS = NEW HTTPOPTIONS
    DIM r AS FETCHRESULT
    g_proxied = g_proxied + 1
    url = "http://127.0.0.1:" + LTRIM$(STR$(g_apiPort)) + pathQuery
    opts.Method = method
    IF LEN(body) > 0 THEN opts.Body = body
    opts.Headers.Add("Content-Type", "application/json")
    r = FETCH(url, opts)
    IF r.StatusCode = 0 THEN
        ApiCall = ErrorJson("agent_api_unreachable", "the agent server (:" + LTRIM$(STR$(g_apiPort)) + ") is not responding")
    ELSE
        ApiCall = r.Body
    END IF
END FUNCTION

' --- Helper JSON -----------------------------------------------------------

FUNCTION JQ(k AS STRING) AS STRING
    JQ = CHR$(34) + k + CHR$(34)
END FUNCTION

FUNCTION JStr(s AS STRING) AS STRING
    DIM t AS STRING
    t = s
    t = REPLACE$(t, "\", "\\")
    t = REPLACE$(t, CHR$(34), "\" + CHR$(34))
    t = REPLACE$(t, CHR$(10), "\n")
    t = REPLACE$(t, CHR$(13), "")
    t = REPLACE$(t, CHR$(9), " ")
    JStr = CHR$(34) + t + CHR$(34)
END FUNCTION

FUNCTION BoolJson(b AS INTEGER) AS STRING
    IF b = 1 THEN
        BoolJson = "true"
    ELSE
        BoolJson = "false"
    END IF
END FUNCTION

FUNCTION ErrorJson(code AS STRING, msg AS STRING) AS STRING
    ErrorJson = "{" + JQ("error") + ":" + JStr(code) + "," + JQ("message") + ":" + JStr(msg) + "}"
END FUNCTION

' Campo stringa da un body JSON ("" se assente/vuoto/JSON invalido).
FUNCTION JsonField(body AS STRING, key AS STRING) AS STRING
    DIM v AS STRING
    DIM j AS JSON
    JsonField = ""
    IF LEN(STRINGS.TRIM(body)) = 0 THEN EXIT FUNCTION
    j = JSON.PARSE(body)
    v = JSON.EXTRACT(j, key)
    JsonField = v
END FUNCTION

' ============================================================================
' WebSocket canvas (WEBCANVAS: frame JSON → static/canvas.js)
' ============================================================================
SUB CanvasWsHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE, ws AS WEBSOCKET)
    ws.OnMessage = "CanvasOnMessage"
    ws.OnClose = "CanvasOnClose"
    DrawDemoFrame(ws)
END SUB

SUB DrawDemoFrame(ws AS WEBSOCKET)
    WEBCANVAS.Clear(ws, 0)
    WEBCANVAS.PSet(ws, 10, 10, 4)
    WEBCANVAS.Line(ws, 0, 0, 120, 120, 7)
    WEBCANVAS.Rect(ws, 20, 20, 60, 40, 2)
    WEBCANVAS.Circle(ws, 80, 80, 20, 14)
    WEBCANVAS.TextOut(ws, 10, 150, "Hello Canvas", 7)
    WEBCANVAS.Flush(ws)
END SUB

SUB CanvasOnMessage(ws AS WEBSOCKET, msg AS STRING)
    PRINT "[canvas] msg: "; msg
    IF INSTR(STRINGS.UPPER(msg), "DEMO") > 0 THEN DrawDemoFrame(ws)
END SUB

SUB CanvasOnClose(ws AS WEBSOCKET)
    PRINT "[canvas] closed"
END SUB
