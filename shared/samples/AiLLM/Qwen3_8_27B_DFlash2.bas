' PARITY-EXCLUDE: carica il modello LOCALE Qwen3.8-27B-DFlash2; esempio manuale (sentinella). Pesi via motore MLX embedded, SWAP su disco.
' FILE: AiLLM/Qwen3_8_27B_DFlash2.bas
' ESEMPIO: Qwen3.8-27B-DFlash2 (model_type: qwen3) — AI REGISTER col motore MLX embedded di GMBasic.
' swap=true: pesi lazy/file-backed (mmap), paginati dal SO da disco on-demand.
IMPORT AI WITH ALIAS
IMPORT LLM

SUB Main()
    PRINT "Qwen3.8-27B-DFlash2 — motore MLX embedded GMBasic (model_type: qwen3)"
    IF NOT IO.FILEEXISTS("~/ai/Models/.gmbasic-allow-load") THEN
        PRINT "Modello NON caricato (protezione anti-saturazione)."
        PRINT "Per eseguire:  touch ~/ai/Models/.gmbasic-allow-load  e rilancia."
        END
    END IF
    AI REGISTER "qwen3827bdflash2", "completion", "~/ai/Models/Qwen3.8-27B-DFlash2", "{""engine"":""mlx"",""swap"":""true"",""maxTokens"":""32"",""temperature"":""0"",""contextSize"":""4096""}"
    AI SET ACTIVE "completion", "qwen3827bdflash2"
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
