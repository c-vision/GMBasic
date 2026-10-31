' PARITY-EXCLUDE: carica un modello LOCALE; solo manuale (sentinella).
'' FILE: Ai/AiMlxEngineRouting.bas
' =================================================================================================
' COMPLESSO: registrazione provider, attivazione per capability/stringa, lettura dell'attivo e
' agent ask; percorso REALE DS4 via motore MLX protetto da sentinella.
'
' COPERTURA comandi/funzioni:
'   AI REGISTER (comando, con chiave "engine" nel param JSON)
'   AI SET ACTIVE (%CHAT e forma stringa "completion")
'   AI.ACTIVE(%CHAT)
'   AI.AGENTASKSYNC(name, prompt)
'   AI AGENT CREATE / AI AGENT SET (impostazioni deterministiche)
'
' Determinismo: la parte sempre eseguita usa provider HTTP/fixture (nessuna rete, nessun modello);
' la parte REALE DS4 (motore MLX embedded) è protetta dalla sentinella
' /Users/gaetano/ds4/.gmbasic-allow-load ed è fuori dalla parità 5-backend (modello reale).
' =================================================================================================
IMPORT AI WITH ALIAS
IMPORT LLM

SUB Main()
    PRINT "AiMlxEngineRouting"
    PRINT "=================="

    ' 1) AI REGISTER di un provider remoto: solo registrazione (nessuna chiamata di rete),
    '    con la chiave "engine" nel param JSON (override esplicito del motore). AI SET ACTIVE
    '    accetta sia la capability %CHAT sia il ruolo stringa "completion".
    AI REGISTER "rem-http", "completion", "http://127.0.0.1:9/v1", "{""engine"":""openai""}"
    AI SET ACTIVE %CHAT, "rem-http"
    PRINT "attivo1: "; AI.ACTIVE(%CHAT)

    ' 2) Ritorno al provider fixture deterministico (identico su tutti i backend).
    AI SET ACTIVE "completion", "fixture-completion"
    PRINT "attivo2: "; AI.ACTIVE(%CHAT)

    ' 3) Percorso REALE DS4/DeepSeek V4 Flash via MOTORE MLX embedded: registra il .gguf
    '    (auto-route a "mlx") e crea un agente con system prompt minimo. Pesante (~97 GB):
    '    esegue SOLO se esiste la sentinella. Fuori parità (modello reale): tutti i comandi
    '    (AI REGISTER, AI SET ACTIVE, AI.ACTIVE, AI.AGENTASKSYNC, AI AGENT CREATE/SET)
    '    sono esercitati qui.
    IF IO.FILEEXISTS("/Users/gaetano/ds4/.gmbasic-allow-load") THEN
        AI REGISTER "ds4mlx", "completion", "/Users/gaetano/ds4/ds4flash.gguf", "{""engine"":""mlx"",""maxTokens"":""16"",""temperature"":""0"",""contextSize"":""4096""}"
        AI SET ACTIVE %CHAT, "ds4mlx"
        PRINT "attivo-ds4: "; AI.ACTIVE(%CHAT)
        AI AGENT CREATE "ds4agent"
        AI AGENT SET "ds4agent", systemprompt, "Rispondi in modo conciso."
        AI AGENT SET "ds4agent", skills, "OFF"
        AI AGENT SET "ds4agent", memory, "OFF"
        AI AGENT SET "ds4agent", plugins, "OFF"
        AI AGENT SET "ds4agent", maxtokens, "8"
        PRINT "ds4: "; AI.AGENTASKSYNC("ds4agent", "In una riga: qual e' la capitale dell'Italia?")
        AI AGENT DELETE "ds4agent"
    ELSE
        PRINT "ds4: (skip, sentinella assente)"
    END IF

    PRINT "fine"
END SUB
