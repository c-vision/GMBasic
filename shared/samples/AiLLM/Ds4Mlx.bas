' PARITY-EXCLUDE: carica DS4 (~97GB) via motore MLX embedded; solo manuale con sentinella.
' FILE: AI/Ds4Mlx.bas
' ======================================================================================================================
' ESEMPIO MINIMALE: CARICA DeepSeek V4 Flash (DS4) COL MOTORE MLX EMBEDDED DI GMBasic E RISPONDE A UN PROMPT
' ======================================================================================================================
' DS4 è un modello MoE (~97 GB) in un SINGOLO file .gguf. Il REGISTER lo auto-rileva come motore
' MLX (architettura "deepseek4" in ARCH_TABLE): i pesi restano mmap-ati nel file (paginati
' on-demand) e il TOKENIZER è letto dallo stesso GGUF (niente tokenizer.json esterno).
' Per forzare esplicitamente il motore: 4° argomento JSON  {"engine":"mlx"}.
'
' ⚠️ SICUREZZA: il caricamento è PESANTE. Per NON saturare la macchina nei gate automatici
'    (che eseguono TUTTI i sample), avviene SOLO se esiste la sentinella. Per eseguire davvero:
'        touch ~/ds4/.gmbasic-allow-load
'    poi rilanciare. Senza sentinella il sample salta e termina con successo.
' ======================================================================================================================
IMPORT AI WITH ALIAS
IMPORT LLM

SUB Main()
    PRINT "DS4 (DeepSeek V4 Flash) — motore MLX embedded GMBasic"
    PRINT "===================================================="

    IF NOT IO.FILEEXISTS("~/ds4/.gmbasic-allow-load") THEN
        PRINT "DS4 NON caricato (protezione anti-saturazione)."
        PRINT "Per eseguire: crea ~/ds4/.gmbasic-allow-load e rilancia."
        PRINT "Percorso modello: ~/ds4/ds4flash.gguf"
        PRINT
        PRINT "Test completato (skip sicuro)."
        END
    END IF

    ' 1. Registra il modello: il .gguf di DS4 è auto-rilevato dal motore MLX (arch deepseek4).
    '    contextSize/maxTokens sono le chiavi del provider MLX (temperature=0 → greedy).
    AI REGISTER "ds4mlx", "completion", "~/ds4/ds4flash.gguf", "{""maxTokens"":""32"",""temperature"":""0"",""contextSize"":""4096""}"
    AI SET ACTIVE "completion", "ds4mlx"
    PRINT "Attivo: "; AI.ACTIVE(%CHAT)

    ' 2. Prompt semplicissimo.
    '    NB: l'agente inietta un system prompt (default ~2 KB + skills/memoria) e usa il SUO
    '    MaxTokens (default 2048), ignorando il maxTokens del provider. Con DS4 (prefill ~0.7 s/
    '    token su CPU) un system prompt grande = minuti. Qui: system minimo, skills/memory/plugins
    '    OFF, maxtokens basso → risposta in tempi ragionevoli.
    AI AGENT CREATE "ds4mlxagent"
    AI AGENT SET "ds4mlxagent", systemprompt, "Rispondi in modo conciso."
    AI AGENT SET "ds4mlxagent", skills, "OFF"
    AI AGENT SET "ds4mlxagent", memory, "OFF"
    AI AGENT SET "ds4mlxagent", plugins, "OFF"
    AI AGENT SET "ds4mlxagent", maxtokens, "16"
    DIM ris$ AS STRING
    ris$ = AI.AGENTASKSYNC("ds4mlxagent", "In una riga: qual e' la capitale dell'Italia?")
    PRINT "ds4 (MLX) dice: "; ris$
    AI AGENT DELETE "ds4mlxagent"

    PRINT
    PRINT "Test completato."
END SUB
