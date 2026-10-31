' PARITY-EXCLUDE: server MCP esterni (Connection refused :8899 se assenti; catalogo/connessioni variano) — il gap validazione VMC resta coperto da McpFullSurface (2026-09-23, Parita-5-Backend-Residui §2B)
' FILE: AI/McpServerListClient.bas
' =================================================================================================
' Lista server MCP come CLIENT (namespace AI, nessun pacchetto esterno):
'   - AI MCP SERVER ADD nome, kind, source | AI MCP SERVER ADD ...  : aggiunge un server alla lista
'       kind = "http"  -> source = URL (JSON-RPC)
'       kind = "stdio" -> source = comando gia' sul PATH
'   - AI MCP SERVER ENABLE/DISABLE nome     | AI MCP SERVER ENABLE ... : abilita/disabilita
'   - AI.MCPSERVERLIST() / AI.MCPSERVERSTATUS(nome) : letture
'   - ris$ = AI.MCPCALL(server, tool, args$)         : invoca un tool del server
'   - AI MCP SERVER MARKETPLACE GET nome | AI MCP SERVER MARKETPLACE GET : aggiunge dal catalogo e
'     scopre i tool (initialize + tools/list); AI.MCPSERVERMARKETPLACELIST() elenca il catalogo
' =================================================================================================
IMPORT AI WITH ALIAS

SUB Main()
    ' --- 0. Default: alla prima esecuzione ~/.gmbasic/mcp.json e' pre-popolato con i server
    '     piu' comuni del catalogo (filesystem, fetch, memory, git, time, ...).
    PRINT "=== CATALOGO MARKETPLACE ==="
    PRINT AI.MCPSERVERMARKETPLACELIST()
    PRINT

    ' --- 0bis. Aggiunge un server dal catalogo e scopre i tool (richiede l'eseguibile,
    '     es. npx; se assente il GET fallisce a runtime).
    ' AI MCP SERVER MARKETPLACE GET filesystem

    ' --- 1. Aggiunge un server HTTP (JSON-RPC) alla lista client ---
    AI MCP SERVER ADD "files", "http", "http://127.0.0.1:8899/mcp"
    AI MCP SERVER ENABLE "files"
    PRINT "Stato: "; AI.MCPSERVERSTATUS("files")
    PRINT "Lista:"; AI.MCPSERVERLIST()
    PRINT

    ' --- 1bis. Scopre i tool del server (tools/list) ---
    PRINT "Tool del server:"
    PRINT AI.MCPLISTTOOLSSERVER("files")
    PRINT

    ' --- 1ter. AUTO-DISCOVERY: gli agenti vedono automaticamente i tool dei server
    '     abilitati, con nome 'server.tool' — nessun AI AGENT REGISTER TOOL necessario.
    '     Il flag 'clienttools' e' OPZIONALE con default OFF (evita il rumore nel prompt);
    '     con ON l'agente vede i tool dei server abilitati.
    AI AGENT CREATE "agente"
    AI AGENT SET "agente" provider "fixture"
 AI AGENT SET "agente" clienttools ON
    PRINT "clienttools = "; AI.AGENTGET("agente", "clienttools")
    ' Con un completer che fa tool-calling (OpenAI/Ollama/Anthropic/MLX), l'agente
    ' puo' chiamare 'files.read_file' direttamente:
    ' DIM risposta$ = AI.AGENTASKSYNC("agente", "leggi /tmp/a.txt con read_file")
    ' PRINT risposta$

    ' --- 2. Invoca un tool del server (se raggiungibile) ---
    DIM ris$ = AI.MCPCALL("files", "read_file", "{""path"":""/tmp/a.txt""}")
    PRINT "CALL read_file -> "; ris$
    PRINT

    ' --- 3. Disabilita e rimuove ---
    AI MCP SERVER DISABLE "files"
    PRINT "Dopo DISABLE: "; AI.MCPSERVERSTATUS("files")
    AI MCP SERVER REMOVE "files"
    PRINT "Dopo REMOVE, lista vuota: "; AI.MCPSERVERLIST()

    PRINT "Test completato."
END SUB
