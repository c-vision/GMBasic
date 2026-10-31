IMPORT AI WITH ALIAS
IMPORT LLM
SUB Main()
    PRINT "1) PRUNE olmoe 64->32 esperti"
    LLM PRUNE "/Users/gaetano/ai/Models/gguf/olmoe-1b-7b-0924-instruct-q4_k_m.gguf", "work/olmoe_pruned2.gguf", 32
    PRINT "2) SAVE"
    LLM MODEL SAVE "work/olmoe_pruned2.gguf", "work/olmoe_pruned2_saved.gguf"
    PRINT "3) CONVERT MLX"
    LLM MODEL CONVERT "work/olmoe_pruned2_saved.gguf", "work/olmoe_pruned2_mlx", "MLX"
    PRINT "4) genera"
    AI REGISTER "pr", "completion", "work/olmoe_pruned2_mlx", "{""engine"":""mlx"",""swap"":""true"",""maxTokens"":""32"",""temperature"":""0"",""contextSize"":""2048""}"
    AI SET ACTIVE "completion", "pr"
    AI AGENT CREATE "ag"
    AI AGENT SET "ag", maxtokens, "32"
    DIM r$ AS STRING
    r$ = AI.AGENTASKSYNC("ag", "Rispondi in una riga: qual e' la capitale dell'Italia?")
    PRINT "RISPOSTA: "; r$
    AI AGENT DELETE "ag"
    PRINT "OK"
END SUB
