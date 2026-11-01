' PARITY-EXCLUDE: server esterno + browser/CDP (client C#-only)
' FILE: AI/McpBrowserServer.bas
' =================================================================
' §5 Camoufox — MCP browser server: espone l'automazione browser nativa
' (Chromium/Obscura via CDP) come tool MCP raggiungibili da un client ESTERNO
' (non BASIC: Claude Desktop, IDE, qualunque client MCP 2024-11-05).
'
' I tool browser_* NON passano da wrapper FUNCTION BASIC: il dispatcher
' IlMcpToolDispatcher instrada browser_* → IlRuntime.BrowserToolDispatch
' (BrowserRuntime → CDP, percorso puro C#). La registrazione qui serve solo a
' materializzare i tool in McpToolRegistry (tools/list) e la loro InputSchema.
'
' Uso:
'   export GMBASIC_MCP_API_KEY=segreta        ' http richiede la chiave
'   dotnet run --project REPL -- samples/Ai/McpBrowserServer.bas
'     → registra i tool e serve MCP su http://127.0.0.1:8765/mcp
'       (Authorization: Bearer $KEY)
'
' Client esterno minimo:
'   K=$GMBASIC_MCP_API_KEY; M=http://127.0.0.1:8765/mcp
'   curl -s $M -H "Authorization: Bearer $K" -d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","clientInfo":{"name":"curl","version":"1"}}}'
'   curl -s $M -H "Authorization: Bearer $K" -d '{"jsonrpc":"2.0","id":2,"method":"tools/list"}'
'   curl -s $M -H "Authorization: Bearer $K" -d '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"browser_navigate","arguments":{"url":"https://example.com"}}}'
'   curl -s $M -H "Authorization: Bearer $K" -d '{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"browser_text"}}'
'   curl -s $M -H "Authorization: Bearer $K" -d '{"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"browser_click","arguments":{"selector":"@e2"}}}'
' =================================================================
IMPORT AI WITH ALIAS

SUB Main()
  ' Registra i tool (procName = nome tool → dispatcher nativo browser_*).
  AI MCP REGISTER TOOL "browser_navigate",     "browser_navigate",     "Naviga a un URL (Chromium/Obscura via CDP)"
  AI MCP REGISTER TOOL "browser_snapshot",     "browser_snapshot",     "Snapshot DOM grezzo della pagina corrente"
  AI MCP REGISTER TOOL "browser_text",         "browser_text",         "Snapshot DOM (alias testo/pagina)"
  AI MCP REGISTER TOOL "browser_accesstree",   "browser_accesstree",   "Snapshot accessibility compatto @eN (ref stabili)"
  AI MCP REGISTER TOOL "browser_click",        "browser_click",        "Click su selettore CSS o ref @eN"
  AI MCP REGISTER TOOL "browser_fill",         "browser_fill",         "Compila un campo (selector, value)"
  AI MCP REGISTER TOOL "browser_evaluate",     "browser_evaluate",     "Esegue JavaScript e ritorna il valore"
  AI MCP REGISTER TOOL "browser_screenshot",   "browser_screenshot",   "Screenshot PNG → path file (opz. path)"
  AI MCP REGISTER TOOL "browser_get_cookies",  "browser_get_cookies",  "Elenca i cookie del browser"
  AI MCP REGISTER TOOL "browser_set_cookie",   "browser_set_cookie",   "Imposta un cookie (name, value[, url])"
  AI MCP REGISTER TOOL "browser_clear_cookies","browser_clear_cookies","Svuota i cookie del browser"
  AI MCP REGISTER TOOL "browser_set_header",   "browser_set_header",   "Aggiunge un header HTTP (name, value)"
  AI MCP REGISTER TOOL "browser_set_user_agent","browser_set_user_agent","Override dello User-Agent"
  AI MCP REGISTER TOOL "browser_human_click",  "browser_human_click",  "Click human-like (Input.dispatchMouseEvent)"
  AI MCP REGISTER TOOL "browser_human_fill",   "browser_human_fill",   "Fill human-like (per-char delay)"
  AI MCP REGISTER TOOL "browser_human_scroll", "browser_human_scroll", "Scroll con rotella mouse (dy)"
  AI MCP REGISTER TOOL "browser_start",        "browser_start",        "Avvia/verifica provider CDP (auto|chromium|obscura[, port])"

  DIM tools$ AS STRING
  tools$ = AI.MCPLISTTOOLS()
  PRINT "Registered browser MCP tools:"
  PRINT tools$

  ' Serve MCP in HTTP (richiede GMBASIC_MCP_API_KEY in env). Client esterno: curl/Claude.
  AI MCP SERVE "http", 8765
  PRINT "MCP browser server su http://127.0.0.1:8765/mcp"
  SLEEP 60000
END SUB

