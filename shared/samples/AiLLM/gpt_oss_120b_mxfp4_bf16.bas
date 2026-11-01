' PARITY-EXCLUDE: carica il modello LOCALE gpt-oss-120b-mxfp4-bf16; esempio manuale (sentinella). Pesi via motore MLX embedded, SWAP su disco.
' FILE: AiLLM/gpt_oss_120b_mxfp4_bf16.bas
' ESEMPIO: gpt-oss-120b-mxfp4-bf16 (model_type: gpt_oss) — AI REGISTER col motore MLX embedded di GMBasic.
' swap=true: pesi lazy/file-backed (mmap), paginati dal SO da disco on-demand.
IMPORT AI WITH ALIAS
IMPORT LLM

SUB Main()
    PRINT "gpt-oss-120b-mxfp4-bf16 — motore MLX embedded GMBasic (model_type: gpt_oss)"
    IF NOT IO.FILEEXISTS("~/ai/Models/.gmbasic-allow-load") THEN
        PRINT "Modello NON caricato (protezione anti-saturazione)."
        PRINT "Per eseguire:  touch ~/ai/Models/.gmbasic-allow-load  e rilancia."
        END
    END IF
    AI REGISTER "gptoss120bmxfp4bf16", "completion", "~/ai/Models/gpt-oss-120b-mxfp4-bf16", "{""engine"":""mlx"",""swap"":""true"",""maxTokens"":""32"",""temperature"":""0"",""contextSize"":""4096""}"
    AI SET ACTIVE "completion", "gptoss120bmxfp4bf16"
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
