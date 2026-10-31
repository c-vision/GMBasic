' PARITY-EXCLUDE: diagnostica engine speculativo solo C# (dflash2): i contatori/parametri del motore C non espongono gli stessi dati
' FILE: Ai/AiSpeculativeDflash2.bas
' =================================================================
' DFLASH2 SPECULATIVE DA BASIC (surface completa, 2026-09-02)
' =================================================================
' Abilita lo speculative decoding col draft block-diffusion DFlash2
' (Qwen3.8-27B-DFlash2) bind-ato al target Qwen3.8-27B-8bit sul motore MLX.
'
' Come funziona la registrazione:
'   AI REGISTER <alias>, %CHAT, <path TARGET>, "<cfg json>"
'     - <path TARGET> = cartella repo MLX del modello chat (auto-detect kind "mlx" e
'       modelDir); le chiavi del cfg json vengono MERGED sopra la config auto-detectata
'       (modelDir resta quello del path) -- merge implementato in RegisterSource.
'     - cfg json chiavi reali: "speculation":"dflash" (round-loop nativo),
'       "draftModelDir":"<path draft DFlash2>", "temperature":"0" (il dflash e' greedy).
'
' Il decode DFlash2 e' LOSSLESS: output == greedy del target (target authoritative),
' con draft block-diffusion che accelera il decode (misurato ~1.9x su 120 token;
' accettanza ~51% vs 70% del reference -- item di allineamento numerico aperto).
'
' Richiede i modelli locali reali (editare i path):
'   target: ~/ai/Models/Qwen3.8-27B-8bit
'   draft : ~/ai/Models/Qwen3.8-27B-DFlash2
' (senza, il REGISTER fallisce con errore esplicito gestito da ON ERROR.)
' =================================================================
IMPORT AI WITH ALIAS
IMPORT LLM

SUB Main()
    ON ERROR GOTO errore

    PRINT "DFlash2 speculative da BASIC"
    PRINT "============================"
    AI REGISTER "dfl2", %CHAT, "/Users/gaetano/ai/Models/Qwen3.8-27B-8bit", _
        "{""draftModelDir"":""/Users/gaetano/ai/Models/Qwen3.8-27B-DFlash2"",""speculation"":""dflash"",""temperature"":""0"",""maxTokens"":""48""}"
    AI SET ACTIVE %CHAT, "dfl2"
    PRINT "registrato+attivo: dfl2 (speculation=dflash, draft DFlash2)"

    AI AGENT CREATE "a1"
    AI AGENT SET "a1" "maxtokens" "48"
    DIM r$ = AI.AGENTASKSYNC("a1", "The capital of France is")
    PRINT "risposta: "; r$
    AI AGENT DELETE "a1"
    PRINT "OK"
    END

errore:
    PRINT "ERRORE (modelli locali non presenti o registrazione fallita): "
END SUB
