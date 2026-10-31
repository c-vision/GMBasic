' PARITY-EXCLUDE: carica il modello LOCALE DeepSeek-V4.1-Flash; esempio manuale (sentinella). Pesi via motore MLX embedded, SWAP su disco.
' FILE: AiLLM/DeepSeek_V4_1_Flash.bas
' ESEMPIO: DeepSeek-V4.1-Flash (model_type: deepseek_v41) — AI REGISTER col motore MLX embedded di GMBasic.
' swap=true: pesi lazy/file-backed (mmap), paginati dal SO da disco on-demand.
IMPORT AI WITH ALIAS
IMPORT LLM

SUB Main()
    PRINT "DeepSeek-V4.1-Flash — motore MLX embedded GMBasic (model_type: deepseek_v41)"
    IF NOT IO.FILEEXISTS("/Users/gaetano/ai/Models/.gmbasic-allow-load") THEN
        PRINT "Modello NON caricato (protezione anti-saturazione)."
        PRINT "Per eseguire:  touch /Users/gaetano/ai/Models/.gmbasic-allow-load  e rilancia."
        END
    END IF
    AI REGISTER "deepseekv41flash", "completion", "/Users/gaetano/ai/Models/DeepSeek-V4.1-Flash", "{""engine"":""mlx"",""swap"":""true"",""maxTokens"":""32"",""temperature"":""0"",""contextSize"":""4096""}"
    AI SET ACTIVE "completion", "deepseekv41flash"
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
