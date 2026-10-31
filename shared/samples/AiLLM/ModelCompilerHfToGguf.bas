' PARITY-EXCLUDE: conversione + round-trip su modello LOCALE pesante (solo manuale).
' FILE: AiLLM/ModelCompilerHfToGguf.bas
' =====================================================================================
' EMITTER FORTE (§6 crit.2): da un checkpoint HF/MLX si EMETTE un GGUF (metodo antirez:
' template metadata dall'input + byte rigenerati), lo si VALIDA e se ne prova il ROUND-TRIP
' (GGUF -> MLX) GENERANDO dal modello ri-emesso.
'   1. CONVERT HF/MLX -> GGUF   (converter codec + .llmarch/.gmbarch)
'   2. VALIDATE                 (integrita' strutturale dell'artefatto emesso)
'   3. CONVERT GGUF -> MLX      (round-trip)
'   4. genera                   (fedelta' del round-trip)
' Prerequisito: DeepSeek-V2-Lite-Chat-4bit-mlx in ~/ai/Models.
' NOTA: dipende dal FALLBACK F16 del converter (commit b4f6afd) per gli esperti quantizzati
' (switch_mlp.down_proj, cols non multiplo di 256).
' =====================================================================================
IMPORT AI WITH ALIAS
IMPORT LLM

SUB Main()
    DIM src$ AS STRING
    src$ = "/Users/gaetano/ai/Models/DeepSeek-V2-Lite-Chat-4bit-mlx"
    PRINT "=== EMITTER forte §6 crit.2 — HF/MLX -> GGUF -> MLX -> genera ==="
    IF NOT IO.FILEEXISTS(src$ + "/config.json") THEN
        PRINT "sorgente assente: serve DeepSeek-V2-Lite-Chat-4bit-mlx in ~/ai/Models"
        END
    END IF
    PRINT "1) EMIT CONVERT HF/MLX -> GGUF"
    LLM MODEL CONVERT src$, "work/hf2gguf.gguf", "GGUF"
    PRINT "2) VALIDATE (artefatto emesso)"
    LLM MODEL VALIDATE "work/hf2gguf.gguf", "work/hf2gguf_val.txt"
    PRINT "3) ROUND-TRIP GGUF -> MLX"
    LLM MODEL CONVERT "work/hf2gguf.gguf", "work/hf2gguf_mlx", "MLX"
    PRINT "4) genera dal modello ri-emesso (round-trip fedele)"
    AI REGISTER "rt", "completion", "work/hf2gguf_mlx", "{""engine"":""mlx"",""swap"":""true"",""maxTokens"":""32"",""temperature"":""0"",""contextSize"":""2048"",""eosToken"":""<｜end▁of▁sentence｜>""}"
    AI SET ACTIVE "completion", "rt"
    AI AGENT CREATE "ag"
    AI AGENT SET "ag", maxtokens, "32"
    DIM r$ AS STRING
    r$ = AI.AGENTASKSYNC("ag", "Rispondi in una riga in italiano: qual e' la capitale dell'Italia?")
    PRINT "RISPOSTA: "; r$
    AI AGENT DELETE "ag"
    PRINT "OK: EMITTER forte (HF/MLX -> GGUF -> MLX -> genera)."
END SUB
