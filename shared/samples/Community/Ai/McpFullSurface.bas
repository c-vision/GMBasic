' ==============================================================================
' FILE: Ai/McpFullSurface.bas
' MCP — SUPERFICIE COMPLETA (esempio completo e complesso)
' ==============================================================================
' Esercita OGNI comando e funzione del dominio MCP su GMBasic:
'
'   comandi : AI MCP REGISTER TOOL (firma completa 5 arg + firma corta)
'             AI MCP UNREGISTER TOOL · AI MCP REGISTER AGENT
'             AI MCP SERVE · AI MCP STOP
'             AI MCP SERVER ADD/ENABLE/DISABLE/REFRESH/REMOVE · AI MCP SERVER MARKETPLACE GET
'   funzioni: AI.MCPLISTTOOLS() · AI.MCPRESOURCES() · AI.MCPPROMPTS()
'             AI.MCPLISTTOOLSSERVER() · AI.MCPCALL()
'             AI.MCPSERVERLIST() · AI.MCPSERVERSTATUS() · AI.MCPSERVERMARKETPLACELIST()
'
' Deterministico e senza rete: le parti dipendenti dall'ambiente (lista server della
' configurazione locale, marketplace) sono usate ma NON stampate; si stampano solo
' esiti booleani/conteggi stabili. Il server MCP REALE (stdio/http) è verificato
' end-to-end da `tools/test_mcp_e2e.sh` — qui `AI MCP SERVE` usa un mode invalido che
' ritorna 0 senza bloccare (un `AI MCP SERVE "stdio"` bloccherebbe su stdin).
'
' Parità: le stesse righe di output su IL, PCodeVM, VM/C, C99Native.
' ==============================================================================

IMPORT AI

' --- ACCESS LAYER: la procedura esposta come tool ---
FUNCTION AddCents(amountA AS LONG, amountB AS LONG) AS STRING
    AddCents = STR$(amountA + amountB) + "c"
END FUNCTION

SUB Main()
    ' ============================ COMANDI: registrazione ========================
    ' Firma completa: tool, proc, descrizione, READONLY, outputSchema (JSON).
    AI MCP REGISTER TOOL "add_cents", "AddCents", "Somma due importi in centesimi", TRUE, "{""type"":""string""}"
    ' Firma corta (solo tool+proc): gli altri campi prendono i default.
    AI MCP REGISTER TOOL "echo", "AddCents"
    ' Un agente diventa un tool `agent-<nome>`.
    AI MCP REGISTER AGENT "analista"

    ' ============================ FUNZIONI: lettura =============================
    DIM tools AS STRING = AI.MCPLISTTOOLS()
    PRINT "tool_registrato="; IIF(INSTR(tools, "add_cents") > 0, 1, 0)
    PRINT "tool_corto="; IIF(INSTR(tools, "echo") > 0, 1, 0)
    PRINT "readonly="; IIF(INSTR(tools, "readOnly") > 0, 1, 0)
    PRINT "outputschema="; IIF(INSTR(tools, "outputSchema") > 0, 1, 0)
    PRINT "inputschema="; IIF(INSTR(tools, "amountA") > 0, 1, 0)
    PRINT "agent_tool="; IIF(INSTR(tools, "agent-analista") > 0, 1, 0)

    DIM res AS STRING = AI.MCPRESOURCES()
    PRINT "resource_uri="; IIF(INSTR(res, "basic://tools/add_cents") > 0, 1, 0)
    PRINT "resource_names="; IIF(INSTR(res, "add_cents") > 0, 1, 0)

    DIM prm AS STRING = AI.MCPPROMPTS()
    PRINT "prompts_json="; IIF(LEN(prm) >= 2, 1, 0)

    ' ============================ FUNZIONI: server ==============================
    PRINT "status_ignoto="; LEN(AI.MCPSERVERSTATUS("non_esiste"))
    DIM lts AS STRING = AI.MCPLISTTOOLSSERVER("non_esiste")
    PRINT "listtools_err="; IIF(INSTR(lts, "non configurato") > 0, 1, 0)
    DIM cal AS STRING = AI.MCPCALL("non_esiste", "un_tool", "{}")
    PRINT "call_err="; IIF(INSTR(cal, "non configurato") > 0, 1, 0)
    ' Marketplace: usato ma non stampato (dipende dalla distribuzione).
    DIM mkt AS STRING = AI.MCPSERVERMARKETPLACELIST()
    PRINT "marketplace_json="; IIF(LEN(mkt) >= 2, 1, 0)

    ' ============================ COMANDI: server ================================
    AI MCP SERVER ADD "demo_full_surface", "stdio", "echo"
    AI MCP SERVER ENABLE "demo_full_surface"
    ' MCPSERVERLIST legge la CONFIGURAZIONE persistita (mcp.json): il suo contenuto dipende
    ' dalla config dell'utente e dalla cartella dati risolta a runtime (CWD) — la funzione
    ' si USA ma NON si asserisce/stampa nulla che ne dipenda (sample deterministico).
    DIM srv AS STRING = AI.MCPSERVERLIST()
    AI MCP SERVER DISABLE "demo_full_surface"
    AI MCP SERVER REFRESH
    AI MCP SERVER MARKETPLACE GET "non_esiste"
    AI MCP SERVER REMOVE "demo_full_surface"
    PRINT "server_ops=ok"

    ' ============================ COMANDI: ciclo di vita ========================
    AI MCP UNREGISTER TOOL "echo"
    DIM tools2 AS STRING = AI.MCPLISTTOOLS()
    PRINT "unregister="; IIF(INSTR(tools2, "echo") = 0, 1, 0)

    ' Serving: mode invalido → ritorna 0 senza bloccare (il serving vero è nell'E2E).
    AI MCP SERVE "bogus"
    AI MCP STOP
    PRINT "serving=ok"

    ' Vetrina locale del tool (l'executor MCP la usa identica via tools/call).
    PRINT "add_cents="; AddCents(2500, 1250)
END SUB
