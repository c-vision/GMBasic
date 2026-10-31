' PARITY-EXCLUDE: carica il modello LOCALE DeepSeek-V2-Lite-Chat-4bit-mlx; esempio manuale (sentinella). Pesi via motore MLX embedded, SWAP su disco.
' FILE: AiLLM/DeepSeek_V2_Lite_Chat_4bit_mlx.bas
' ESEMPIO: DeepSeek-V2-Lite-Chat-4bit-mlx (model_type: deepseek_v2) — AI REGISTER col motore MLX embedded di GMBasic.
' swap=true: pesi lazy/file-backed (mmap), paginati dal SO da disco on-demand.
IMPORT AI WITH ALIAS
IMPORT LLM

SUB Main()
    PRINT "DeepSeek-V2-Lite-Chat-4bit-mlx — motore MLX embedded GMBasic (model_type: deepseek_v2)"
    IF NOT IO.FILEEXISTS("/Users/gaetano/ai/Models/.gmbasic-allow-load") THEN
        PRINT "Modello NON caricato (protezione anti-saturazione)."
        PRINT "Per eseguire:  touch /Users/gaetano/ai/Models/.gmbasic-allow-load  e rilancia."
        END
    END IF
    AI REGISTER "deepseekv2litechat4bitml", "completion", "/Users/gaetano/ai/Models/DeepSeek-V2-Lite-Chat-4bit-mlx", "{""engine"":""mlx"",""swap"":""true"",""maxTokens"":""32"",""temperature"":""0"",""contextSize"":""4096""}"
    AI SET ACTIVE "completion", "deepseekv2litechat4bitml"
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
