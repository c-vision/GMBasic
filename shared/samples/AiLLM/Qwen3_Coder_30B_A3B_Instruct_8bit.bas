' PARITY-EXCLUDE: carica il modello LOCALE Qwen3-Coder-30B-A3B-Instruct-8bit; esempio manuale (sentinella). Pesi via motore MLX embedded, SWAP su disco.
' FILE: AiLLM/Qwen3_Coder_30B_A3B_Instruct_8bit.bas
' ESEMPIO: Qwen3-Coder-30B-A3B-Instruct-8bit (model_type: qwen3_moe) — AI REGISTER col motore MLX embedded di GMBasic.
' swap=true: pesi lazy/file-backed (mmap), paginati dal SO da disco on-demand.
IMPORT AI WITH ALIAS
IMPORT LLM

SUB Main()
    PRINT "Qwen3-Coder-30B-A3B-Instruct-8bit — motore MLX embedded GMBasic (model_type: qwen3_moe)"
    IF NOT IO.FILEEXISTS("/Users/gaetano/ai/Models/.gmbasic-allow-load") THEN
        PRINT "Modello NON caricato (protezione anti-saturazione)."
        PRINT "Per eseguire:  touch /Users/gaetano/ai/Models/.gmbasic-allow-load  e rilancia."
        END
    END IF
    AI REGISTER "qwen3coder30ba3binstruct", "completion", "/Users/gaetano/ai/Models/Qwen3-Coder-30B-A3B-Instruct-8bit", "{""engine"":""mlx"",""swap"":""true"",""maxTokens"":""32"",""temperature"":""0"",""contextSize"":""4096""}"
    AI SET ACTIVE "completion", "qwen3coder30ba3binstruct"
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
