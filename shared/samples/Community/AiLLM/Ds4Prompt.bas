' PARITY-EXCLUDE: carica un modello LOCALE; solo manuale (sentinella).
'' FILE: AI/Ds4Prompt.bas
' ======================================================================================================================
' ESEMPIO: CARICA DS4 (DeepSeek-V4-Flash) LOCALE ED ESEGUE UN PROMPT
' ======================================================================================================================
' ds4 è un modello MoE da ~91 GB su disco, esposto come GGUF e caricato dal provider llama.cpp
' locale (llama.cpp usa mmap: i pesi restano su disco e vengono paginati on-demand, non tutti
' in RAM). Il percorso è LOCALE e HARDCODED.
'
' ⚠️ SICUREZZA: il caricamento di ds4 è PESANTE (91 GB, paginazione). Per NON saturare la macchina
'    nei gate automatici (che eseguono TUTTI i sample), il caricamento avviene SOLO se esiste la
'    sentinella scelta dall'utente. Per eseguire davvero:
'        touch /Users/gaetano/ds4/.gmbasic-allow-load
'    poi rilanciare questo sample. Senza sentinella il sample salta e termina con successo.
' ======================================================================================================================
IMPORT AI WITH ALIAS
IMPORT LLM

SUB Main()
    PRINT "ds4 (DeepSeek-V4-Flash) locale + prompt"
    PRINT "======================================"

    IF NOT IO.FILEEXISTS("/Users/gaetano/ds4/.gmbasic-allow-load") THEN
        PRINT "ds4 NON caricato (protezione anti-saturazione)."
        PRINT "Per eseguire: crea /Users/gaetano/ds4/.gmbasic-allow-load e rilancia."
        PRINT "Percorso modello: /Users/gaetano/ds4/ds4flash.gguf"
        PRINT
        PRINT "Test completato (skip sicuro)."
        END
    END IF

    ' 1. Registra ds4 come provider completion locale (GGUF → llama.cpp, mmap).
    '    gpuLayers=0 → mmap + CPU: gli esperti restano su disco (paginati on-demand), la RAM
    '    resta bassa (verificato: ~53 GB di pagina cache mmap, 46 GB liberi, NESSUNA saturazione).
    '    gpuLayers=-1 (tutti i layer su Metal) è più veloce MA carica gli esperti in memoria
    '    unificata (~86 GB su ds4): usalo solo se la RAM libera basta. Se il modello NON entra
    '    nella memoria richiesta, il caricamento deve fallire con errore esplicito (mai saturare).
    AI REGISTER "ds4", "completion", "/Users/gaetano/ds4/ds4flash.gguf", "{""maxTokens"":""32"",""temperature"":""0"",""ctxSize"":""4096"",""gpuLayers"":""0""}"
    AI SET ACTIVE "completion", "ds4"
    PRINT "Attivo: "; AI.ACTIVE(%CHAT)

    ' 2. Esegue un prompt.
    AI AGENT CREATE "ds4agent"
    DIM ris$ AS STRING
    ris$ = AI.AGENTASKSYNC("ds4agent", "In una riga: qual e' la capitale dell'Italia?")
    PRINT "ds4 dice: "; ris$

    PRINT
    PRINT "Test completato."
END SUB
