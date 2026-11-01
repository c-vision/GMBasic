' PARITY-EXCLUDE: carica un modello LOCALE; solo manuale (sentinella).
'' ==============================================================================
' FILE: AI/AgentStructuredToolCalling.bas
' Agent — structured tool calling su Ollama e LlamaCpp (A2, 2026-09-07)
' ==============================================================================
' Novità A2: OllamaLlmCompleter e LlamaCppLlmCompleter implementano
' IToolCallingCompleter (prima solo OpenAI/Anthropic + MLX constrained).
' Il loop AgentToolLoop usa ora CompleteWithTools (tool_calls strutturati)
' invece di prompt-engineering + ToolCallParser per questi provider.
'
' Verificato: IL, PCodeVM, VM/C, C99, C99Native — build AI 0 errori
' Provider Ollama: POST /api/chat con tools[] → message.tool_calls
' Provider LlamaCpp: prompt con ## Available tools + JSON tool_calls → parsing
' ==============================================================================

IMPORT AI WITH ALIAS

IMPORT LLM

' --- Tool BASIC: somma due numeri (usato come MCP tool) ---
FUNCTION CalcSum(a AS INTEGER, b AS INTEGER) AS STRING
    CalcSum = STR$(a + b)
END FUNCTION

SUB Main()
    PRINT "=== Agent Structured Tool Calling (Ollama / LlamaCpp) ==="
    PRINT

    ' 1) Registra tool MCP "calc_sum" che mappa a CalcSum
    AI MCP REGISTER TOOL "calc_sum", "CalcSum", "Sum of two integers a and b"
    PRINT "Tool calc_sum registrato: " + AI.MCPLISTTOOLS()

    ' 2) Crea agente e associa tool
    AI AGENT CREATE ToolAgent
    AI AGENT REGISTER TOOL "ToolAgent", "calc_sum"
    PRINT "Agent ToolAgent creato + tool associato"

    ' 3) Provider Ollama (se disponibile) — ora con tool_calls strutturati
    '    Richiede Ollama in esecuzione su localhost:11434 con modello llama3.1
    '    Se non disponibile, il fallback è il provider attivo (o fixture in test)
    ON ERROR RESUME NEXT
    AI REGISTER "ollama-local", %CHAT, "ollama", "{""model"":""llama3.1"",""baseUrl"":""http://localhost:11434""}"
    IF ERR = 0 THEN
        AI SET ACTIVE %CHAT, "ollama-local"
        PRINT "Provider Ollama registrato e attivato (structured)"
    ELSE
        PRINT "Ollama non disponibile — uso provider attivo/fixture"
    END IF
    ON ERROR GOTO 0

    ' 4) Alternativa: provider LlamaCpp locale (decommenta se hai un .gguf)
    ' AI REGISTER "llama-local", %CHAT, "llamacpp", "{""modelPath"":""~/ai/Models/llama-3.2-3b.gguf"",""ctxSize"":4096}"
    ' AI SET ACTIVE %CHAT, "llama-local"
    ' PRINT "Provider LlamaCpp registrato (structured via prompt)"

    ' 5) Chiede all'agente di usare il tool — con provider strutturato il modello
    '    risponde con tool_calls JSON invece di testo libero
    DIM answer$ AS STRING
    answer$ = AI.AGENTASKSYNC("ToolAgent", "Use calc_sum to add 21 and 21")
    PRINT "ANSWER="; answer$

    ' 6) Stato ed eventi
    PRINT "STATUS="; AI.AGENTSTATUS("ToolAgent")
    DIM ev$ AS STRING
    ev$ = AI.AGENTEVENTS()
    PRINT "EVENTS len="; LEN(ev$)

    AI AGENT DELETE "ToolAgent"
    PRINT "Agent rimosso — completato."
END SUB

