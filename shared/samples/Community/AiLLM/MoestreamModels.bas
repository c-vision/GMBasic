' PARITY-EXCLUDE: carica un modello LOCALE; solo manuale (sentinella).
'' FILE: AI/MoestreamModels.bas
' ======================================================================================================================
' GUIDA TEORICA: provider AI embedded "moestream" — MoE OUT-OF-CORE
' ======================================================================================================================
' Il provider "moestream" esegue modelli MoE (Mixture-of-Experts) più grandi della RAM tenendo
' gli esperti SU DISCO e caricandoli on-demand. Punti chiave:
'
' 1. PIÙ MODELLI IN MEMORIA
'    Più modelli possono essere caricati contemporaneamente (nome → modello). Ogni modello ha
'    la propria policy (cache, prefetch, device) e il proprio stato.
'
' 2. SWAP SU DISCO (DEFAULT)
'    `LLM MODEL LOAD nome, path` carica in modalità SWAP: gli esperti restano nel container
'    `.gmoe` e vengono letti on-demand (streaming out-of-core). La RAM usata è piccola e
'    costante rispetto alla dimensione del modello.
'
' 3. RESIDENT (NO SWAP) — forzato dal BASIC
'    `LLM MODEL LOAD nome, path, RESIDENT` precarica TUTTI gli esperti in RAM: nessun accesso
'    a disco durante l'inferenza. Utile quando il modello sta in memoria e si vuole il massimo.
'
' 4. POLICY PER-MODELO
'    `LLM MODEL CACHE nome, MB`      budget della cache esperti (0 = disabilitata)
'    `LLM MODEL PREFETCH nome, N`    prefetch cross-layer di N esperti/layer (0 = OFF)
'    `LLM MODEL DEVICE nome, MODO`   AUTO | CPU | METAL (METAL fallisce se il device non c'è)
'    `LLM MODEL UNLOAD nome`         libera un modello senza toccare gli altri
'    `LLM MODEL PROMOTE nome`        → ACTIVE (tutti gli esperti in RAM, M21)
'    `LLM MODEL DEMOTE nome`         → WARM (esperti on-demand, M21)
'    `LLM MODEL CONVERT src, dst`    converte un checkpoint HF/MLX in `.gmoe` (offline, GUARDED)
'    `AI SET ACTIVE %CHAT, nome`    rende il modello il provider attivo per gli agenti
'    `AI SET PRIORITY nome, N`      priorità di eviction (più alto = tenuto di più, M21)
'
' 5. STATISTICHE
'    `LLM.STREAMSTATS(nome)` ritorna JSON: mode, device, byte letti da disco, byte residenti,
'    hit-rate della cache, accuratezza del prefetch.
'
' NOTA: l'esecuzione dei sample gira sul backend IL (il provider nativo è una dylib P/Invoke).
' ======================================================================================================================
IMPORT AI WITH ALIAS
IMPORT LLM

SUB Main()
    PRINT "moestream: modelli MoE out-of-core (piu' modelli in memoria)"
    PRINT "=========================================================="
    PRINT

    ' 1. DEFAULT: SWAP su disco (streaming degli esperti).
    LLM MODEL LOAD m1, "/Users/gaetano/Documents/Projects/GMBasicMonoRepo/shared/samples/Fixtures/models/tiny.gmoe"
    PRINT "m1 (swap)      : "; LLM.STREAMSTATS("m1")

    ' 2. RESIDENT forzato dal BASIC: tutto in RAM, nessuno swap.
    LLM MODEL LOAD m2, "/Users/gaetano/Documents/Projects/GMBasicMonoRepo/shared/samples/Fixtures/models/tiny.gmoe", RESIDENT
    PRINT "m2 (resident)  : "; LLM.STREAMSTATS("m2")
    PRINT

    ' 3. Policy per-modello: cache + prefetch + device.
    LLM MODEL CACHE m1, 64
    LLM MODEL PREFETCH m1, 2
    LLM MODEL DEVICE m1, CPU
    PRINT "m1 con policy  : "; LLM.STREAMSTATS("m1")
    PRINT

    ' 3b. Residenza (M21): PROMOTE → ACTIVE (tutti gli esperti in RAM), DEMOTE → WARM (on-demand).
    LLM MODEL PROMOTE m1
    PRINT "m1 ACTIVE      : "; LLM.STREAMSTATS("m1")
    LLM MODEL DEMOTE m1
    PRINT "m1 WARM        : "; LLM.STREAMSTATS("m1")

    ' 3c. Priorità (M21): AI SET PRIORITY → eviction dal più basso (più alto = tenuto di più).
    AI SET PRIORITY "m1", 100
    PRINT "m1 WARM        : "; LLM.STREAMSTATS("m1")
    PRINT

    ' 4. I due modelli convivono: scaricarne uno non tocca l'altro.
    LLM MODEL UNLOAD m1
    PRINT "dopo unload m1 : "; LLM.STREAMSTATS("m2")
    PRINT

    LLM MODEL UNLOAD m2
    PRINT

    ' 5. Un modello locale si gestisce SOLO con LLM MODEL: `LLM MODEL LOAD` registra da sé il
    '    provider "moestream" (AI REGISTER è per i provider HTTP/API, non per i modelli).
    LLM MODEL LOAD mlocal, "/Users/gaetano/Documents/Projects/GMBasicMonoRepo/shared/samples/Fixtures/models/tiny.gmoe"
    PRINT "mlocal: "; LLM.STREAMSTATS("mlocal")
    LLM MODEL UNLOAD mlocal

    ' 6. AI SET ACTIVE: rende un modello caricato il provider attivo per gli agenti.
    LLM MODEL LOAD mact, "/Users/gaetano/Documents/Projects/GMBasicMonoRepo/shared/samples/Fixtures/models/tiny.gmoe"
    AI SET ACTIVE %CHAT, "mact"
    PRINT "attivo         : "; AI.ACTIVE(%CHAT)
    LLM MODEL UNLOAD mact
    PRINT

    ' 7. LLM MODEL CONVERT (GUARDED): converte un checkpoint HF/MLX in `.gmoe` (OFFLINE, pesante).
    '    Gira solo se esiste la sentinella (i gate eseguono TUTTI i sample) e il converter è
    '    raggiungibile (GMB_MODEL_CONVERT/PATH/build). Senza sentinella salta.
    IF IO.FILEEXISTS("/tmp/gmbasic-moestream-convert.allow") THEN
        LLM MODEL CONVERT "/Users/gaetano/ai/Models/Qwen3-Coder-30B-A3B-Instruct-8bit", "/tmp/gmbasic-moestream-convert-all.gmoe"
        PRINT "CONVERT: container prodotto in /tmp/gmbasic-moestream-convert-all.gmoe"
    ELSE
        PRINT "CONVERT saltato (crea /tmp/gmbasic-moestream-convert.allow per eseguirlo)."
    END IF

    PRINT "Test completato."
END SUB
