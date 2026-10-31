' PARITY-EXCLUDE: carica un modello LOCALE; solo manuale (sentinella).
'' FILE: AI/TwoLocalModels.bas
' ======================================================================================================================
' ESEMPIO COMPLESSO: DUE MODELLI LOCALI CARICATI CONTEMPORANEAMENTE
' ======================================================================================================================
' GMBasic tiene PIÙ modelli locali in memoria allo stesso tempo: ogni modello è un provider
' distinto (nome → modello) con la propria configurazione. Qui se ne caricano DUE (entrambi
' modelli MLX locali già presenti su disco), si mostrano entrambi nel registro, si sceglie
' quello attivo e gli si chiede un prompt.
'
' - I percorsi sono LOCALI e HARDCODED (nessuna installazione Python/server).
' - Lo stesso schema vale per GGUF/llama.cpp e MLX.
' ======================================================================================================================
IMPORT AI WITH ALIAS
IMPORT LLM

SUB Main()
    PRINT "Due modelli locali in memoria contemporaneamente"
    PRINT "==============================================="

    ' Portabilita': se i modelli locali non ci sono, salta senza crashare.
    IF NOT IO.DIREXISTS("/Users/gaetano/ai/Models/Llama-3.2-1B-Instruct-4bit") THEN
        PRINT "Modello locale assente: salto. Percorso atteso:"
        PRINT "/Users/gaetano/ai/Models/Llama-3.2-1B-Instruct-4bit"
        END
    END IF
    IF NOT IO.DIREXISTS("/Users/gaetano/ai/Models/Ternary-Bonsai-4B-mlx-2bit") THEN
        PRINT "Modello locale assente: salto. Percorso atteso:"
        PRINT "/Users/gaetano/ai/Models/Ternary-Bonsai-4B-mlx-2bit"
        END
    END IF

    ' 1. Registra DUE modelli locali (entrambi restano in memoria; percorsi hardcoded).
    AI REGISTER "llama1b", "completion", "/Users/gaetano/ai/Models/Llama-3.2-1B-Instruct-4bit", "{""maxTokens"":""24"",""temperature"":""0""}"
    AI REGISTER "bonsai", "completion", "/Users/gaetano/ai/Models/Ternary-Bonsai-4B-mlx-2bit", "{""maxTokens"":""24"",""temperature"":""0""}"

    ' 2. Elenca i provider completion registrati (entrambi i modelli sono presenti).
    PRINT "Provider completion: "; AI.LIST("completion")

    ' 3. Scegli il modello attivo e interrogalo con un prompt.
    AI SET ACTIVE "completion", "llama1b"
    PRINT "Attivo: "; AI.ACTIVE(%CHAT)

    AI AGENT CREATE "assistente"
    DIM ris$ AS STRING
    ris$ = AI.AGENTASKSYNC("assistente", "Rispondi in una sola riga: quanto fa 2+2?")
    PRINT "llama1b dice: "; ris$

    ' 4. Passa all'altro modello SENZA scaricare il primo (resta in memoria).
    AI SET ACTIVE "completion", "bonsai"
    PRINT "Attivo: "; AI.ACTIVE(%CHAT)
    ris$ = AI.AGENTASKSYNC("assistente", "Rispondi in una sola riga: quanto fa 3+3?")
    PRINT "bonsai dice: "; ris$

    PRINT
    PRINT "Test completato."
END SUB
