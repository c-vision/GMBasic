' PARITY-EXCLUDE: model surgery su modello LOCALE pesante (sentinella anti-saturazione).
' FILE: AiLLM/ModelSurgeryPrune.bas
' =====================================================================================
' MODEL SURGERY §4.1 — RIMUOVERE: il ciclo completo end-to-end.
'   1. PRUNE      : riduce gli esperti routed (router coerente) 64 -> 48
'   2. MODEL SAVE : EMITTER — scrive l'artefatto (GGUF)
'   3. CONVERT    : GGUF -> MLX (dir caricabile dal motore)
'   4. genera     : il modello POTATO carica e genera
'
' Prerequisito: work/dsv2l_mxfp4.gguf (dalla catena ModelCompilerMlxToDs4.bas).
' Prova il ciclo di surgery importato <=> potato: lo stesso codice vale per ogni MoE.
' =====================================================================================
IMPORT AI WITH ALIAS
IMPORT LLM

SUB Main()
    PRINT "=== Model Surgery §4.1 — PRUNE -> SAVE -> CONVERT -> genera ==="
    IF NOT IO.FILEEXISTS("work/dsv2l_mxfp4.gguf") THEN
        PRINT "assente work/dsv2l_mxfp4.gguf (esegui prima ModelCompilerMlxToDs4.bas)"
        END
    END IF
    PRINT "1) PRUNE 64 -> 48 esperti (router coerente)"
    LLM PRUNE "work/dsv2l_mxfp4.gguf", "work/dsv2l_p48.gguf", 48
    PRINT "2) MODEL SAVE (EMITTER)"
    LLM MODEL SAVE "work/dsv2l_p48.gguf", "work/dsv2l_p48s.gguf"
    PRINT "3) CONVERT GGUF -> MLX"
    LLM MODEL CONVERT "work/dsv2l_p48s.gguf", "work/dsv2l_p48_mlx", "MLX"
    PRINT "4) carica e genera col modello POTATO"
    AI REGISTER "p48", "completion", "work/dsv2l_p48_mlx", "{""engine"":""mlx"",""swap"":""true"",""maxTokens"":""32"",""temperature"":""0"",""contextSize"":""2048"",""eosToken"":""<｜end▁of▁sentence｜>""}"
    AI SET ACTIVE "completion", "p48"
    AI AGENT CREATE "ag"
    AI AGENT SET "ag", maxtokens, "32"
    DIM r$ AS STRING
    r$ = AI.AGENTASKSYNC("ag", "Rispondi in una riga in italiano: qual e' la capitale dell'Italia?")
    PRINT "RISPOSTA: "; r$
    AI AGENT DELETE "ag"
    PRINT "OK: il modello potato genera (surgery end-to-end)."
END SUB
