' EXPECT: swap-model
' MLX swap su disco — AI REGISTER con "engine":"mlx" e "swap":"true": i pesi del modello MLX
' restano LAZY/file-backed (mmap) e il SO li pagina da disco on-demand, invece di materializzarli
' tutti al load. Chiavi accettate: swap / diskSwap / lazyWeights. Qui si verifica che la
' registrazione del provider con l'opzione swap sia valida e che l'attivo sia quello scelto
' (nessun modello viene caricato nel test). Parità 4 backend.
IMPORT AI
IMPORT LLM

SUB Main()
    AI REGISTER "swap-model", "completion", "/Users/gaetano/ai/Models/Llama-3.2-1B-Instruct-4bit", "{""engine"":""mlx"",""swap"":""true""}"
    AI SET ACTIVE %CHAT, "swap-model"
    PRINT AI.ACTIVE(%CHAT)
END SUB
