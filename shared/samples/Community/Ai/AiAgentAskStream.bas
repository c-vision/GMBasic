' FILE: AI/AiAgentAskStream.bas
' =================================================================================================
' Streaming agente:
'   - AI.AGENTASKSYNC(nome, prompt)          : interroga l'agente (sincrono) -> STRING.
'   - AI AGENT ASK STREAM nome, prompt INTO Sub : streaming per-token, Sub(t$) per ogni chunk.
' - AI AGENT SET nome sessionstreaming ON : streaming per-token anche nelle sessioni native
'                                              (ON/OFF opzionale, default OFF; solo se il binding
'                                              lo supporta).
' =================================================================================================
IMPORT AI WITH ALIAS

AI AGENT CREATE "assistente"

' --- 1. Sincrono ---
DIM ris$ = AI.AGENTASKSYNC("assistente", "Rispondi in una riga.")
PRINT "Sincrono: "; ris$
PRINT

' --- 2. Streaming (comando con callback) ---
PRINT "Streaming per token:"
AI AGENT ASK STREAM "assistente", "Conta da 1 a 3." INTO OnToken
SLEEP 5000
PRINT
PRINT

' --- 3. Proprieta' agente ---
PRINT "sessionstreaming = "; AI.AGENTGET("assistente", "sessionstreaming")
AI AGENT SET "assistente" sessionstreaming ON
PRINT "sessionstreaming (ON) = "; AI.AGENTGET("assistente", "sessionstreaming")
PRINT

PRINT "Test completato."
END

' Callback: riceve ogni token/delta della risposta streaming.
SUB OnToken(t$ AS STRING)
    PRINT t$;
END SUB
