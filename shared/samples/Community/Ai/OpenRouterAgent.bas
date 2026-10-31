' FILE: Ai/OpenRouterAgent.bas
' =================================================================================================
' ESEMPIO COMPLETO: agente GMBasic che usa un modello REMOTO via OpenRouter (free), senza alcun
' modello locale. Copre: AI REGISTER (provider HTTP), AI SET KEY, AI SET ACTIVE, AI.ACTIVE,
' AI AGENT CREATE / AI AGENT SET, AI.AGENTASKSYNC.
'
' La API key OpenRouter NON è committata: valorizza `orKey$` qui sotto per eseguire; lasciandola
' vuota il sample salta (nessuna rete, nessun modello). Modello di esempio: "openrouter/free"
' (router che sceglie dinamicamente un modello free di OpenRouter).
'
' PARITY-EXCLUDE: rete esterna (OpenRouter) — output dipendente dal modello remoto/free pool
' (rate-limit 429, modelli diversi), non confrontabile fra i 4 backend in CI.
' =================================================================================================
IMPORT AI WITH ALIAS

SUB Main()
    PRINT "OpenRouterAgent (modello remoto)"
    PRINT "================================"

    DIM orKey$ AS STRING
    orKey$ = ""          ' <-- metti qui la tua API key OpenRouter (sk-or-...) per eseguire

    IF orKey$ = "" THEN
        PRINT "API key OpenRouter non impostata: skip (nessun modello caricato, nessuna rete)."
        PRINT "Per eseguire: valorizza orKey$ nella riga sopra e rilancia."
        END
    END IF

    ' 1) Provider remoto OpenAI-compatibile: URL + modello (l'apiKey si imposta a parte).
    AI REGISTER "or", "completion", "https://openrouter.ai/api/v1", "openrouter/free"
    AI SET KEY "or", orKey$
    AI SET ACTIVE "completion", "or"
    PRINT "attivo: "; AI.ACTIVE(%CHAT)

    ' 2) Agente con settings deterministici e system prompt minimo.
    AI AGENT CREATE "oragent"
    AI AGENT SET "oragent", systemprompt, "Rispondi in modo conciso."
    AI AGENT SET "oragent", skills, "OFF"
    AI AGENT SET "oragent", memory, "OFF"
    AI AGENT SET "oragent", plugins, "OFF"
    AI AGENT SET "oragent", maxtokens, "64"

    ' 3) Ask sincrono: il modello remoto risponde (nessun modello locale).
    PRINT "risposta: "; AI.AGENTASKSYNC("oragent", "In una riga: qual e' la capitale dell'Italia?")
    AI AGENT DELETE "oragent"

    PRINT "fine"
END SUB
