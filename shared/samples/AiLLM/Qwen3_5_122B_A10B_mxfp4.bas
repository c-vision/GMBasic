' PARITY-EXCLUDE: carica il modello LOCALE Qwen3.5-122B-A10B-mxfp4; esempio manuale (sentinella). Pesi via motore MLX embedded, SWAP su disco.
' FILE: AiLLM/Qwen3_5_122B_A10B_mxfp4.bas
' ESEMPIO: Qwen3.5-122B-A10B-mxfp4 (model_type: qwen3_5_moe) — AI REGISTER col motore MLX embedded di GMBasic.
' swap=true: pesi lazy/file-backed (mmap), paginati dal SO da disco on-demand.
IMPORT AI WITH ALIAS
IMPORT LLM

SUB Main()
    PRINT "Qwen3.5-122B-A10B-mxfp4 — motore MLX embedded GMBasic (model_type: qwen3_5_moe)"
    IF NOT IO.FILEEXISTS("~/ai/Models/.gmbasic-allow-load") THEN
        PRINT "Modello NON caricato (protezione anti-saturazione)."
        PRINT "Per eseguire:  touch ~/ai/Models/.gmbasic-allow-load  e rilancia."
        END
    END IF
    AI REGISTER "qwen35122ba10bmxfp4", "completion", "~/ai/Models/Qwen3.5-122B-A10B-mxfp4", "{""engine"":""mlx"",""swap"":""true"",""maxTokens"":""32"",""temperature"":""0"",""contextSize"":""4096""}"
    AI SET ACTIVE "completion", "qwen35122ba10bmxfp4"
    PRINT "Attivo: "; AI.ACTIVE(%CHAT)
    AI AGENT CREATE "ag"
    AI AGENT SET "ag", systemprompt, "Rispondi in modo conciso."
    AI AGENT SET "ag", skills, "OFF"
    AI AGENT SET "ag", memory, "OFF"
    AI AGENT SET "ag", plugins, "OFF"
    AI AGENT SET "ag", maxtokens, "32"
    DIM ris$ AS STRING
    ris$ = AI.AGENTASKSYNC("ag", "In una riga: qual e' la capitale dell'Italia?")
    PRINT "dice: "; ris$
    AI AGENT DELETE "ag"
END SUB
