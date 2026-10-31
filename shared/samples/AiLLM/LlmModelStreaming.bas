' FILE: AILLM/LlmModelStreaming.bas
' ======================================================================================================================
' LLM MODEL — RESIDENZA E STREAMING OUT-OF-CORE DI UN MODELLO MoE (GGUF)
' ======================================================================================================================
' Dimostra TUTTI i verbi del comando `LLM MODEL` e la funzione `LLM.STREAMSTATS`:
'
'     LLM MODEL LOAD    "<nome>", "<path.gguf>" [, RESIDENT]   ' carica (default SWAP; RESIDENT=tutto in RAM)
'     LLM MODEL UNLOAD  "<nome>"                                ' scarica il modello
'     LLM MODEL CACHE   "<nome>", <MB>                          ' budget della expert cache (streaming)
'     LLM MODEL PREFETCH "<nome>", <topn>                       ' prefetch speculativo degli esperti
'     LLM MODEL DEVICE  "<nome>", AUTO|CPU|METAL                ' device di calcolo
'     LLM MODEL PROMOTE "<nome>"                                ' provenienza ACTIVE (tutti gli esperti in RAM)
'     LLM MODEL DEMOTE  "<nome>"                                ' residenza WARM (esperti on-demand da disco)
'     LLM.STREAMSTATS("<nome>")                                 ' FUNZIONE → JSON delle statistiche di streaming
'
' I comandi sono la controparte di SCRITTURA; la funzione `LLM.STREAMSTATS` è la controparte di
' LETTURA (ritorna un valore, con parentesi). È il modello "comando ≠ funzione" della direttiva.
'
' IMPORTANTE — questi comandi gestiscono il container **GGUF** (runtime MoE out-of-core
' `gmb_moe_runtime`): con `SWAP` gli esperti sono letti on-demand da disco (mmap + cache LRU a
' bundle gate+up+down); con `RESIDENT` l'intero modello è tenuto in RAM. Per i modelli **MLX**
' (safetensors) lo swap dei pesi si attiva invece con `AI REGISTER ... "{""swap"":""true""}"`.
'
' L'esempio usa una fixture GGUF MoE minima e deterministica (percorso relativo alla cartella del
' sample), così gira su tutti e 5 i backend senza un modello reale. Per un modello reale basta
' sostituire MODEL_PATH con il percorso di un .gguf MoE (es. Qwen3-MoE).
' ======================================================================================================================

IMPORT LLM

' NB: il nome del modello e il percorso sono argomenti di COMANDO: vanno passati come stringhe
' letterali (un identificatore non quotato è già una stringa letterale, per la convenzione dei
' comandi AI/LLM). Per questo il percorso è inline e non una variabile.
' (Fixture relativa alla cartella del sample: samples/Fixtures/models/tiny_moe.gguf.)

' Legge LLM.STREAMSTATS e stampa in forma leggibile il modo e la residenza correnti.
SUB DumpStats(nome AS STRING)
    DIM s AS STRING
    s = LLM.STREAMSTATS(nome)
    ' Il JSON è prodotto dal runtime C: modo (swap/resident) e residenza (warm/active).
    IF INSTR(s, "swap") > 0 THEN
        PRINT "    modo      = swap (esperti on-demand da disco)"
    ELSE
        PRINT "    modo      = resident (tutti gli esperti in RAM)"
    END IF
    IF INSTR(s, "active") > 0 THEN
        PRINT "    residenza = active (tutti gli esperti residenti)"
    ELSE
        PRINT "    residenza = warm (dense residente, esperti on-demand)"
    END IF
END SUB

SUB Main()
    PRINT "================================================================"
    PRINT " LLM MODEL — residenza & streaming out-of-core (GGUF MoE)"
    PRINT "================================================================"

    PRINT "1) LOAD (default SWAP: streaming degli esperti da disco)"
    LLM MODEL LOAD "moe", "../../Fixtures/models/tiny_moe.gguf"
    DumpStats("moe")

    PRINT "2) DEMOTE (residenza WARM: esperti on-demand)"
    LLM MODEL DEMOTE "moe"
    DumpStats("moe")

    PRINT "3) CACHE 32 MB, PREFETCH topn=6, DEVICE CPU"
    LLM MODEL CACHE "moe", 32
    LLM MODEL PREFETCH "moe", 6
    LLM MODEL DEVICE "moe", "CPU"

    PRINT "4) PROMOTE (residenza ACTIVE: precarica tutti gli esperti in RAM)"
    LLM MODEL PROMOTE "moe"
    DumpStats("moe")

    PRINT "5) DEMOTE di nuovo (torna WARM: libera la cache degli esperti)"
    LLM MODEL DEMOTE "moe"

    PRINT "6) UNLOAD (scarica il modello)"
    LLM MODEL UNLOAD "moe"

    PRINT "7) UNLOAD ripetuto: errore CATTURABILE (mai stub silenzioso)"
    ON ERROR GOTO AlreadyGone
    LLM MODEL UNLOAD "moe"
    PRINT "    (nessun errore: inatteso)"
    GOTO Reload
AlreadyGone:
    PRINT "    errore catturato: il modello non e' caricato"
Reload:
    PRINT "8) LOAD con clausola RESIDENT (tutto il modello in RAM: nessuno streaming)"
    LLM MODEL LOAD "moe", "../../Fixtures/models/tiny_moe.gguf", RESIDENT
    DumpStats("moe")
    LLM MODEL UNLOAD "moe"
    PRINT "Fatto."
END SUB
