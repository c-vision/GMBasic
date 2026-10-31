' PARITY-EXCLUDE: carica un modello LOCALE; solo manuale (sentinella).
'' FILE: AI/MoestreamConvert.bas
' ======================================================================================================================
' ESEMPIO: CONVERSIONE di un checkpoint HF/MLX in un container `.gmoe`, poi USO del modello.
' ======================================================================================================================
' Mostra il flusso completo della superficie `LLM MODEL`:
'   1. CONVERT   — converte un checkpoint HuggingFace/MLX in un `.gmoe` (processo esterno, offline)
'   2. LOAD      — carica il container prodotto (e registra da sé il provider "moestream")
'   3. CACHE/PREFETCH/DEVICE — policy del modello caricato
'   4. STREAMSTATS — statistiche (byte letti, cache, prefetch)
'   5. SET ACTIVE — rende il modello attivo per gli agenti (AI AGENT ASK)
'   6. UNLOAD    — libera il modello
'   7. LOAD su CARTELLA — converte automaticamente in <dir>/<nome>.gmoe e carica
'
' ⚠️ SICUREZZA: la conversione è PESANTE (scrive decine di GB e richiede il converter
'    `gmb_model_convert`, modulo del compilatore C). Per NON saturare la macchina nei gate
'    automatici (che eseguono TUTTI i sample) la parte pesante gira SOLO se esiste la sentinella:
'        touch /tmp/gmbasic-moestream-convert.allow
'    e il converter è raggiungibile (env GMB_MODEL_CONVERT o nel PATH).
'    Senza sentinella il sample salta e termina con successo.
' ======================================================================================================================
IMPORT AI WITH ALIAS
IMPORT LLM

SUB Main()
    PRINT "moestream: conversione + uso modello"
    PRINT "===================================="

    IF NOT IO.FILEEXISTS("/tmp/gmbasic-moestream-convert.allow") THEN
        PRINT "Conversione NON eseguita (protezione anti-saturazione)."
        PRINT "Per eseguire:  touch /tmp/gmbasic-moestream-convert.allow"
        PRINT "e assicurati che `gmb_model_convert` sia in PATH o via GMB_MODEL_CONVERT."
        PRINT "Test completato (skip sicuro)."
        END
    END IF

    ' 1. Conversione ESPLICITA: checkpoint HF/MLX → container `.gmoe` con QUANTIZZAZIONE (offline).
    '    Forma: "GMOE:<dense>:<expert>" (es. dense Q8_0, esperti Q2_K/IQ2_XXS — come gguf-tools);
    '    "GMOE" da solo usa i default (q8_0/q2_k). "GGUF"/"MLX" restano i formati bidirezionali.
    LLM MODEL CONVERT "/Users/gaetano/ai/Models/Qwen3-Coder-30B-A3B-Instruct-8bit", "/tmp/gmbasic-moestream-convert.gmoe", "GMOE:q8_0:q2_k"
    PRINT "conversione completata: /tmp/gmbasic-moestream-convert.gmoe"

    ' 2. Carica il container prodotto (registra da sé il provider "moestream": niente AI REGISTER).
    LLM MODEL LOAD "conv", "/tmp/gmbasic-moestream-convert.gmoe"

    ' 3. Policy del modello.
    LLM MODEL CACHE "conv", 512
    LLM MODEL PREFETCH "conv", 2
    LLM MODEL DEVICE "conv", CPU

    ' 4. Statistiche.
    PRINT LLM.STREAMSTATS("conv")

    ' 5. Rendilo attivo per gli agenti (AI AGENT ASK userebbe tokenizer+chat template embeddati).
    AI SET ACTIVE %CHAT, "conv"
    PRINT "modello attivo: "; AI.ACTIVE(%CHAT)

    ' 6. Libera il modello (gli altri modelli resterebbero in memoria).
    LLM MODEL UNLOAD "conv"

    ' 7. Variante AUTOMATICA: dare a LOAD una CARTELLA HF/MLX la converte in <dir>/<nome>.gmoe e la carica.
    LLM MODEL LOAD "auto", "/Users/gaetano/ai/Models/Qwen3-Coder-30B-A3B-Instruct-8bit"
    PRINT LLM.STREAMSTATS("auto")
    LLM MODEL UNLOAD "auto"

    PRINT "Test completato."
END SUB
