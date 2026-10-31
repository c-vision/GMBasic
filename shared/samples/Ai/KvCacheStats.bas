' FILE: Ai/KvCacheStats.bas
' =================================================================
' LLM.CACHESTATS — lettura delle statistiche della KV-cache di un agente.
'
' La funzione opera sul CONTESTO KV risolto per nome (KvCacheContextResolver):
' oggi il nome è un AGENTE registrato (la sua sessione KV via il completer
' cache-manager, es. llama.cpp). Ritorna JSON:
'   {"context":"<nome>","nCtx":N,"nTokens":N,"bytes":N,"posMin":N,"posMax":N,"usedPercent":P}
'
' Comportamento (mai crash):
'   - nome non registrato o agente senza sessione/completer cache-manager
'     -> JSON {"error":"..."} esplicito.
'
' Con un provider locale (llamacpp) configurato, dopo un AIAGENT.ASK la
' sessione dell'agente esiste e le stats sono reali. Esempio di registrazione
' (richiede un modello GGUF locale, path reale):
'   AI REGISTER "local", %CHAT, "llamacpp", "{""modelPath"":""/path/to/model.gguf"",""maxTokens"":""64""}"
'   AI SET ACTIVE %CHAT, "local"
' =================================================================
IMPORT AI WITH ALIAS
IMPORT LLM

SUB Main()
    PRINT "LLM.CACHESTATS — KV-cache di un agente"
    PRINT "======================================="
    PRINT

    ' 1. Agente non registrato -> JSON d'errore esplicito (non crash).
    DIM err$ = LLM.CACHESTATS("nessun-agente")
    PRINT "senza agente: "; err$
    PRINT

    ' 2. Agente registrato ma senza sessione ancora -> error (nessuna sessione).
    '    (Serve un provider cache-manager attivo per la parte 3.)
    AI AGENT CREATE "cache-agent"
    DIM noSession$ = LLM.CACHESTATS("cache-agent")
    PRINT "agente senza sessione: "; noSession$
    PRINT

    ' 3. Se un provider locale e' attivo, un ASK crea la sessione e riempie
    '    la KV-cache; poi le stats sono reali. Senza provider, ASK fallisce
    '    con errore esplicito (gestito da ON ERROR).
    ON ERROR GOTO salta_ask
    AI AGENT SET "cache-agent" "maxtokens" "32"
    DIM risp$ = AI.AGENTASKSYNC("cache-agent", "Rispondi con una sola parola.")
    PRINT "risposta agente: "; risp$
    DIM stats$ = LLM.CACHESTATS("cache-agent")
    PRINT "stats cache: "; stats$
    GOTO fine

salta_ask:
    PRINT "(nessun provider chat attivo — la parte 3 richiede un modello locale registrato)"

fine:
    AI AGENT DELETE "cache-agent"
    PRINT
    PRINT "Test completato."
END SUB
