' SURFACE: esercita la superficie del namespace (drill), non uno scenario didattico.
' FILE: AI/McpUncoveredCoverage.bas
' =================================================================================================
' TEST UNITARIO: MCP - FUNZIONI NON ANCORA COPERTE DAGLI ESEMPI
' =================================================================================================
' Questo test copre le funzioni MCP che non comparivano in alcun esempio .bas:
'   UNREGISTERTOOL, RESOURCES, PROMPTS, REGISTERAGENT.
'
' COPERTURA (namespace AI — non esiste un namespace MCP):
' - AI MCP REGISTER TOOL name, handler, desc : registra tool (già coperto, usato come setup).
' - AI MCP UNREGISTER TOOL tool              : rimuove tool -> BIT.
' - AI.MCPRESOURCES()                        : gestisce le risorse MCP.
' - AI.MCPPROMPTS()                          : gestisce i prompt MCP.
' - AI MCP REGISTER AGENT name               : registra agente -> BIT.
' =================================================================================================
IMPORT AI WITH ALIAS

SUB Main()
    CLS
    PRINT "============================================================"
    PRINT "MCP - Funzioni scoperte dagli esempi"
    PRINT "============================================================"
    PRINT

    ' 1. Setup: registra un tool e un agente
    DIM ok AS BIT
    AI MCP REGISTER TOOL "sum", "SumHandler", "Sums two numbers"
    PRINT "REGISTERTOOL('sum') = "; ok
    AI MCP REGISTER AGENT "assistant"
    PRINT "REGISTERAGENT('assistant') = "; ok
    PRINT

    ' 2. UNREGISTERTOOL
    PRINT "-- UNREGISTERTOOL --"
    AI MCP UNREGISTER TOOL "sum"
    PRINT "UNREGISTERTOOL('sum') = "; ok
    PRINT

    ' 3. RESOURCES e PROMPTS (info sul server MCP)
    PRINT "-- RESOURCES / PROMPTS --"
    PRINT "RESOURCES() = "; AI.MCPRESOURCES()
    PRINT "PROMPTS()   = "; AI.MCPPROMPTS()
    PRINT

    PRINT "Test Completato."
END SUB

' Handler del tool MCP (deve esistere per REGISTERTOOL)
FUNCTION SumHandler(a AS DOUBLE, b AS DOUBLE) AS DOUBLE
    RETURN a + b
END SUB