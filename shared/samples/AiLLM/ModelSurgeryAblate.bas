' PARITY-EXCLUDE: model surgery su modello LOCALE pesante (sentinella anti-saturazione).
' FILE: AiLLM/ModelSurgeryAblate.bas
' =====================================================================================
' MODEL SURGERY §4.1 — MODIFICARE (ABLATE): azzera i tensori per pattern/ruolo.
'   1. ABLATE     : azzera gli esperti (switch_mlp) del layer 17 -> GGUF nuovo
'   2. MODEL SAVE : EMITTER — scrive l'artefatto (GGUF)
'   3. CONVERT    : GGUF -> MLX (dir caricabile dal motore)
'   4. genera     : il modello ABLATO carica e genera (output degradato = la surgery ha agito)
'
' Prerequisito: work/dsv2l_mxfp4.gguf (dalla catena ModelCompilerMlxToDs4.bas).
' Lo stesso meccanismo vale per ogni tensore/ruolo (es. "wo_b", "layers.17.mlp").
' =====================================================================================
IMPORT AI WITH ALIAS
IMPORT LLM

SUB Main()
    PRINT "=== Model Surgery §4.1 — ABLATE -> SAVE -> CONVERT -> genera ==="
    IF NOT IO.FILEEXISTS("work/dsv2l_mxfp4.gguf") THEN
        PRINT "assente work/dsv2l_mxfp4.gguf (esegui prima ModelCompilerMlxToDs4.bas)"
        END
    END IF
    PRINT "1) ABLATE esperti del layer 17 (switch_mlp) -> 0"
    LLM MODEL ABLATE "work/dsv2l_mxfp4.gguf", "work/dsv2l_abl17.gguf", "layers.17.mlp.switch_mlp"
    PRINT "2) MODEL SAVE (EMITTER)"
    LLM MODEL SAVE "work/dsv2l_abl17.gguf", "work/dsv2l_abl17s.gguf"
    PRINT "3) CONVERT GGUF -> MLX"
    LLM MODEL CONVERT "work/dsv2l_abl17s.gguf", "work/dsv2l_abl17_mlx", "MLX"
    PRINT "4) carica e genera col modello ABLATO"
    AI REGISTER "abl17", "completion", "work/dsv2l_abl17_mlx", "{""engine"":""mlx"",""swap"":""true"",""maxTokens"":""32"",""temperature"":""0"",""contextSize"":""2048"",""eosToken"":""<｜end▁of▁sentence｜>""}"
    AI SET ACTIVE "completion", "abl17"
    AI AGENT CREATE "ag"
    AI AGENT SET "ag", maxtokens, "32"
    DIM r$ AS STRING
    r$ = AI.AGENTASKSYNC("ag", "Rispondi in una riga in italiano: qual e' la capitale dell'Italia?")
    PRINT "RISPOSTA: "; r$
    AI AGENT DELETE "ag"
    PRINT "OK: il modello ablato genera (surgery end-to-end)."
END SUB
