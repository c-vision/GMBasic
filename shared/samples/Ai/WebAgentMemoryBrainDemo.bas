' Surface HTTP /api/memory|brain|enrich a PARITA' 4 backend (IL / P-Code / VM/C / C99Native):
' server AI AGENT SERVE implementato in ogni backend (VM/C via gmb_agent_http_start),
' client FETCH in-process senza deadlock (fix watchdog GPM gmb_c99gpm_extjob_*).
' FILE: samples/Community/AI/WebAgentMemoryBrainDemo.bas
' =================================================================
' WEBAPP Memory/Brain/Summary — demo dettagliata (BASIC + HTTP API)
' Copre i 4 blocchi della Log tab: Memory (MEMORY.*), Brain (AI MEMORY*),
' Enrich/Summary (AI.ENRICH) e le API /api/memory|brain|enrich.
' =================================================================
IMPORT MEMORY
IMPORT HTTP
IMPORT JSON
IMPORT AI

SUB Main()
    PRINT "=== WEBAPP Memory/Brain/Summary Demo (dettagliato) ==="
    PRINT ""

    ' ----------------------------------------------------------
    ' 1. MEMORY.* — key-value SQLite :memory: (AgentMemoryStore)
    '    Vedi samples/Community/Ai/MemoryDemo.bas per il base.
    '    API HTTP: POST/GET/DELETE /api/memory + GET /api/memory/search
    ' ----------------------------------------------------------
    PRINT "1) MEMORY.* (key-value)"
    MEMORY.STORE("demo-user", "Ada Lovelace", 0)
    MEMORY.STORE("demo-role", "matematica", 0)
    MEMORY.STORE("demo-tmp", "usa e getta", 0)
    PRINT "   STORE 3 chiavi"
    DIM v$ = MEMORY.LOAD("demo-user")
    PRINT "   LOAD demo-user: " + v$
    DIM s$ = MEMORY.SEARCH("demo-")
    PRINT "   SEARCH demo-: " + REPLACE$(s$, CHR$(10), ", ")
    DIM all$ = MEMORY.LIST()
    PRINT "   LIST: " + REPLACE$(all$, CHR$(10), ", ")
    MEMORY.DELETE("demo-tmp")
    PRINT "   DELETE demo-tmp → LIST: " + REPLACE$(MEMORY.LIST(), CHR$(10), ", ")
    PRINT ""

    ' ----------------------------------------------------------
    ' 2. MEMORY via HTTP API (come fa webapp/pages/index.bsp)
    '    POST /api/memory {"key":"k","value":"v"} → {ok:true}
    '    GET  /api/memory?key=k                → {key, value}
    '    GET  /api/memory                      → {keys:[...]}
    '    GET  /api/memory/search?pattern=demo- → {keys:[...]}
    '    DELETE /api/memory?key=k             → {deleted:true}
    ' ----------------------------------------------------------
    AI AGENT SERVE 8766
    SLEEP 500
    DIM o AS HTTPOPTIONS = NEW HTTPOPTIONS
    o.Method = "POST"
    o.Headers.Add("Content-Type", "application/json")
    o.Body = "{""key"":""http-k1"",""value"":""ciao da API""}"
    DIM r AS FETCHRESULT = FETCH("http://127.0.0.1:8766/api/memory", o)
    PRINT "2) HTTP POST /api/memory: " + r.Body
    DIM r2 AS FETCHRESULT = FETCH("http://127.0.0.1:8766/api/memory?key=http-k1")
    PRINT "   GET ?key=http-k1: " + r2.Body
    DIM r3 AS FETCHRESULT = FETCH("http://127.0.0.1:8766/api/memory")
    PRINT "   GET /api/memory (list): " + r3.Body
    DIM r4 AS FETCHRESULT = FETCH("http://127.0.0.1:8766/api/memory/search?pattern=http-")
    PRINT "   GET /api/memory/search?pattern=http-: " + r4.Body
    DIM d AS HTTPOPTIONS = NEW HTTPOPTIONS
    d.Method = "DELETE"
    DIM r5 AS FETCHRESULT = FETCH("http://127.0.0.1:8766/api/memory?key=http-k1", d)
    PRINT "   DELETE ?key=http-k1: " + r5.Body
    PRINT ""

    ' ----------------------------------------------------------
    ' 3. Second Brain — AI MEMORY NOTE/SAVE/SEARCH + /api/brain/*
    '    Memoria persistente ~/.gmbasic/brain.db (MemoryBrain, SQLite)
    '    BASIC: AI MEMORY NOTE "testo" / AI MEMORY SAVE "fatto" / AI.MEMORYASK(query$, topK)
    '    HTTP:  POST /api/brain/note {"text":"..."} → {id}
    '           POST /api/brain/save {"fact":"..."} → {id}
    '           GET  /api/brain/search?query=...&topK=5 → {result:"id|kind|text"}
    '           GET  /api/brain/list?limit=20          → {count, items:[...]}
    '           POST /api/brain/watch {"path":"/tmp/note.md"} → {watched:[...]}
    '           POST /api/brain/import {"path":"..."}   → {id}
    '           POST /api/brain/refresh                → {refreshed:n}
    '           POST /api/brain/consolidate            → {consolidated:true}
    '           POST /api/brain/forget {"id":n}        → {deleted:true}
    ' ----------------------------------------------------------
    PRINT "3) Second Brain (AI MEMORY + /api/brain/*)"
    ' Nota: AI MEMORY NOTE/SAVE sono comandi, non funzioni — si usano così:
    '   AI MEMORY NOTE "Una nota di test per la dashboard 100%"
    '   AI MEMORY SAVE "Fatto durevole: WEBAPP 8 primitive canvas OK"
    ' Per demo HTTP usiamo le API (stesse che usa index.bsp):
    o.Method = "POST"
    o.Body = "{""text"":""Nota di test Second Brain via demo""}"
    DIM b1 AS FETCHRESULT = FETCH("http://127.0.0.1:8766/api/brain/note", o)
    PRINT "   POST /api/brain/note: " + b1.Body
    o.Body = "{""fact"":""Fatto: WEBAPP Canvas PutImage/GetImage OK""}"
    DIM b2 AS FETCHRESULT = FETCH("http://127.0.0.1:8766/api/brain/save", o)
    PRINT "   POST /api/brain/save: " + b2.Body
    DIM b3 AS FETCHRESULT = FETCH("http://127.0.0.1:8766/api/brain/search?query=WEBAPP&topK=5")
    PRINT "   GET /api/brain/search?query=WEBAPP: " + LEFT$(b3.Body, 400)
    DIM b4 AS FETCHRESULT = FETCH("http://127.0.0.1:8766/api/brain/list?limit=3")
    PRINT "   GET /api/brain/list?limit=3: " + LEFT$(b4.Body, 500)
    o.Body = "{""path"":""/tmp/demo_watch.md""}"
    DIM b5 AS FETCHRESULT = FETCH("http://127.0.0.1:8766/api/brain/watch", o)
    PRINT "   POST /api/brain/watch: " + b5.Body
    DIM b6 AS FETCHRESULT = FETCH("http://127.0.0.1:8766/api/brain/refresh", o)
    PRINT "   POST /api/brain/refresh: " + b6.Body
    DIM b7 AS FETCHRESULT = FETCH("http://127.0.0.1:8766/api/brain/consolidate", o)
    PRINT "   POST /api/brain/consolidate: " + b7.Body
    PRINT ""

    ' ----------------------------------------------------------
    ' 4. Summary — AI.ENRICH / POST /api/enrich
    '    Richiede un provider LLM attivo (AI SET ACTIVE %CHAT, conf),
    '    altrimenti ritorna 200 con summary = "Error: nessun provider..."
    '    Dashboard: textarea + select SUMMARIZATION/TAGGING/ENTITY_EXTRACTION
    ' ----------------------------------------------------------
    PRINT "4) Summary (AI.ENRICH + /api/enrich)"
    DIM txt$ = "GMBasic e una piattaforma BASIC con agenti AI, team, workflow, memoria Second Brain, canvas remoto via WebSocket e target WASM. La WEBAPP unificata espone 5 tab e 11 API per pilotare tutto da browser senza toccare C#."
    DIM je AS JSON = NEW JSON
    je["text"] = txt$
    je["mode"] = "SUMMARIZATION"
    o.Body = JSON.STRINGIFY(je)
    DIM e AS FETCHRESULT = FETCH("http://127.0.0.1:8766/api/enrich", o)
    PRINT "   POST /api/enrich SUMMARIZATION: " + LEFT$(e.Body, 500)
    je["mode"] = "TAGGING"
    o.Body = JSON.STRINGIFY(je)
    DIM e2 AS FETCHRESULT = FETCH("http://127.0.0.1:8766/api/enrich", o)
    PRINT "   POST /api/enrich TAGGING: " + LEFT$(e2.Body, 400)
    AI AGENT STOP SERVER
    PRINT ""

    PRINT "Demo completata — apri webapp/pages/index.bsp Log tab → Memory + Summary."
    PRINT "  Store/Load/Search + Brain Note/Save/Search/Watch/Import/Refresh/Consolidate/List + Enrich"
    PRINT "  sono gli stessi fetch usati in index.bsp (vedi brainNote/brainSave/memStore/doEnrich in JS)."
END SUB

