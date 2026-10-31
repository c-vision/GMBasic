' ==============================================================================
' FILE: Ai/McpToolServerStdio.bas
' McpToolServerStdio — capability server MCP GMBasic servito su stdio
' ==============================================================================
' Programma completo che si comporta come un VERO server MCP: un client esterno
' (es. Claude Desktop, `mcp_e2e_client.py`, l'inspector ufficiale) lo lancia, gli
' parla JSON-RPC 2.0 su stdin/stdout e invoca le procedure BASIC come tool.
'
' È il residuo §3.1 del todo `MCP-Server-GMBasic-From-MCP-001-Pattern`: il serving
' dal mondo C (`gmb_mcp_serve`, runtime condiviso) + l'executor che invoca la
' FUNCTION BASIC. Funziona su IL/PCodeVM e su C99/C99Native (verificato end-to-end
' con `tools/test_mcp_e2e.sh`: initialize → tools/list → tools/call → read-only).
'
' Classificazione READONLY (4° argomento TRUE, pattern MCP-001): il server è avviato
' in modalità read-only, quindi un tool NON dichiarato READONLY viene rifiutato PRIMA
' del dispatch (provato: `mutate` torna errore -32600).
'
' Uso manuale:
'     python3 tools/mcp_e2e_client.py ./work/McpToolServerStdio_<suffisso>
' ==============================================================================

IMPORT AI

' --- DATA/ACCESS LAYER: le procedure che diventano tool MCP ---

' Tool 1 — somma due importi in centesimi (aritmetica intera: identica su 4 backend).
FUNCTION AddCents(amountA AS LONG, amountB AS LONG) AS STRING
    AddCents = STR$(amountA + amountB) + "c"
END FUNCTION

' Tool 2 — totale per id (report compatto multi-argomento).
FUNCTION TotalFor(id AS INTEGER, cents AS LONG) AS STRING
    TotalFor = "id=" + STR$(id) + " total=" + STR$(id * cents)
END FUNCTION

' Tool 3 — NON dichiarato READONLY: in un server read-only viene rifiutato.
FUNCTION MutateItem(id AS INTEGER) AS STRING
    MutateItem = "mutated:" + STR$(id)
END FUNCTION

SUB Main()
    ' MCP LAYER — registrazione dei tool; inputSchema è derivato a compile-time dai
    ' parametri di ciascuna FUNCTION, quindi non va dichiarato a mano.
    ' 5° argomento = outputSchema JSON (opzionale, parità McpToolDefinition.OutputSchema).
    AI MCP REGISTER TOOL "add_cents", "AddCents", "Somma due importi in centesimi", TRUE, "{""type"":""string""}"

    AI MCP REGISTER TOOL "total", "TotalFor", "Totale per id", TRUE
    AI MCP REGISTER TOOL "mutate", "MutateItem", "Tool mutante (non READONLY)"

    ' ⚠️ NOTA (lezione del test end-to-end): in modalità stdio NON si stampa NULLA su
    ' stdout — il canale è il protocollo JSON-RPC e una riga di testo lo corrompe
    ' (il client legge "Tools=..." al posto della risposta). Per la vetrina dei tool
    ' registrati si usa un programma a parte o la modalità HTTP.

    ' Server MCP su stdio in modalità READ-ONLY: blocca su stdin finché il client
    ' non chiude la pipe. Per HTTP: AI MCP SERVE "http", 8765, TRUE
    ' (richiede la variabile d'ambiente GMBASIC_MCP_API_KEY).
    AI MCP SERVE "stdio", 0, TRUE
END SUB
