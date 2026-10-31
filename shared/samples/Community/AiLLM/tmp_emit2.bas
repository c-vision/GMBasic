IMPORT AI WITH ALIAS
IMPORT LLM
SUB Main()
    PRINT "CONVERT + genera"
    LLM MODEL CONVERT "work/dsv2l_emit.gguf", "work/dsv2l_emit_mlx", "MLX"
    AI REGISTER "emit", "completion", "work/dsv2l_emit_mlx", "{""engine"":""mlx"",""swap"":""true"",""maxTokens"":""32"",""temperature"":""0"",""contextSize"":""2048"",""eosToken"":""<｜end▁of▁sentence｜>""}"
    AI SET ACTIVE "completion", "emit"
    AI AGENT CREATE "ag"
    AI AGENT SET "ag", maxtokens, "32"
    DIM r$ AS STRING
    r$ = AI.AGENTASKSYNC("ag", "Rispondi in una riga: qual e' la capitale dell'Italia?")
    PRINT "RISPOSTA: "; r$
    AI AGENT DELETE "ag"
END SUB
