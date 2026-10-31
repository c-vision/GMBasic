' PARITY-EXCLUDE: carica il modello LOCALE DeepSeek-R1-Distill-Qwen-32B-MLX-8Bit; esempio manuale (sentinella). Pesi via motore MLX embedded, SWAP su disco.
' FILE: AiLLM/DeepSeek_R1_Distill_Qwen_32B_MLX_8Bit.bas
' ESEMPIO: DeepSeek-R1-Distill-Qwen-32B-MLX-8Bit (model_type: qwen2) — AI REGISTER col motore MLX embedded di GMBasic.
' swap=true: pesi lazy/file-backed (mmap), paginati dal SO da disco on-demand.
IMPORT AI WITH ALIAS
IMPORT LLM

SUB Main()
    PRINT "DeepSeek-R1-Distill-Qwen-32B-MLX-8Bit — motore MLX embedded GMBasic (model_type: qwen2)"
    IF NOT IO.FILEEXISTS("/Users/gaetano/ai/Models/.gmbasic-allow-load") THEN
        PRINT "Modello NON caricato (protezione anti-saturazione)."
        PRINT "Per eseguire:  touch /Users/gaetano/ai/Models/.gmbasic-allow-load  e rilancia."
        END
    END IF
    AI REGISTER "deepseekr1distillqwen32b", "completion", "/Users/gaetano/ai/Models/DeepSeek-R1-Distill-Qwen-32B-MLX-8Bit", "{""engine"":""mlx"",""swap"":""true"",""maxTokens"":""32"",""temperature"":""0"",""contextSize"":""4096""}"
    AI SET ACTIVE "completion", "deepseekr1distillqwen32b"
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
