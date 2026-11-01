' PARITY-EXCLUDE: carica un modello MLX GIGANTE (~53GB) con SWAP su disco; solo manuale con sentinella.
' FILE: AI/MlxSwapDisk.bas
' ======================================================================================================================
' ESEMPIO MINIMALE: MODELLO MLX PIÙ GRANDE DELLA RAM, CON SWAP DEI PESI SU DISCO
' ======================================================================================================================
' Il motore MLX di GMBasic, di default, PRETOUCHA i pesi (li materializza tutti al load: più
' veloce, ma richiede che il modello entri INTERAMENTE in RAM). Per i modelli giganti che NON
' entrano (es. ~110GB su 128GB) si attiva l'opzione **swap**:
'
'     AI REGISTER "big", "completion", "<dir>", "{""engine"":""mlx"",""swap"":""true""}"
'
' Con swap=true i pesi restano LAZY/file-backed (mmap): il Sistema Operativo li pagina da disco
' on-demand (di fatto uno swap su disco dei pesi) invece di materializzarli tutti al load. Le
' chiavi accettate sono swap / diskSwap / lazyWeights. Default: pretouch (tutto residente).
'
' Equivalenti nativi: `GMB_NO_PRETOUCH_WEIGHTS` (letto per-load, quindi l'opzione è per-modello).
' NB: lo swap vale per i PESI MLX (safetensors). Per la KV/sessione usa `kvSwapEnabled`; per un
' 
' ======================================================================================================================
IMPORT AI WITH ALIAS
IMPORT LLM

SUB Main()
    PRINT "MLX + swap su disco — modello gigante"
    PRINT "===================================="

    IF NOT IO.FILEEXISTS("~/ai/Models/.gmbasic-allow-swap-load") THEN
        PRINT "Modello NON caricato (protezione anti-saturazione)."
        PRINT "Per eseguire:  touch ~/ai/Models/.gmbasic-allow-swap-load  e rilancia."
        PRINT
        PRINT "Test completato (skip sicuro)."
        END
    END IF

    ' swap=true -> pesi lazy/file-backed (mmap), paginati dal SO: il modello non deve entrare tutto in RAM.
    AI REGISTER "bigmlx", "completion", "~/ai/Models/Qwen3.8-Flash-Next-tq4a-tq2e-g64", "{""engine"":""mlx"",""swap"":""true"",""maxTokens"":""24"",""temperature"":""0"",""contextSize"":""4096""}"
    AI SET ACTIVE "completion", "bigmlx"
    PRINT "Attivo: "; AI.ACTIVE(%CHAT)

    AI AGENT CREATE "bigagent"
    AI AGENT SET "bigagent", systemprompt, "Rispondi in modo conciso."
    AI AGENT SET "bigagent", skills, "OFF"
    AI AGENT SET "bigagent", memory, "OFF"
    AI AGENT SET "bigagent", plugins, "OFF"
    AI AGENT SET "bigagent", maxtokens, "24"
    DIM ris$ AS STRING
    ris$ = AI.AGENTASKSYNC("bigagent", "In una riga: qual e' la capitale dell'Italia?")
    PRINT "dice: "; ris$
    AI AGENT DELETE "bigagent"

    PRINT
    PRINT "Test completato."
END SUB
