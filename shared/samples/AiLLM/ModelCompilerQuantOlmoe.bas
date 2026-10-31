' ============================================================================
' ModelCompilerQuantOlmoe.bas — quantizzazione MISTA "stile ds4" di un MoE, in BASIC.
'
' Pipeline del MODEL COMPILER (namespace LLM) su un MoE reale (olmoe-1b-7b:
' 64 esperti, in ~/ai/Models/gguf), con una ricetta MISTA PER-RUOLO in stile
' antirez/DS4:
'
'     ROUTED_EXPERT -> iq2_xxs   (~2.06 bit, guidati dall'importanza/imatrix)
'     FFN           -> q4_k      (FFN densi)
'     ATTENTION     -> q8_0      (proiezioni attenzione)
'     EMBEDDING     -> f16       (nessuna perdita)
'     IMATRIX       -> se presente (altrimenti il quantizer usa l'euristica energetica)
'
' Passi:  PLAN (ispezione) -> POLICY (regole) -> PRUNE (64 -> 32 esperti, router
' coerente) -> QUANT (comprime secondo la policy) -> PLAN del risultato (verifica).
'
' Output in work/ accanto a questo sample. La CWD deve essere QUESTA cartella
' (AGENTS §9/§11). I comandi sono legacy (MAI parentesi); le funzioni hanno i punti.
' NB: negli argomenti dei comandi LLM i termini nudi sono stringhe letterali; i PATH
'     vanno passati come stringhe tra virgolette (le variabili NON sono supportate lì).
' Formati disponibili (case-insensitive): f32 f16 bf16 q4_0 q4_1 q5_0 q5_1 q8_0
'   q2_k q3_k q4_k q5_k q6_k q8_k iq1_s iq1_m iq2_xxs iq2_xs iq2_s iq3_xxs iq3_s
'   iq4_nl iq4_xs tq1_0 tq2_0 mxfp4 nvfp4 q1_0 q2_0.
' ============================================================================
IMPORT LLM
IMPORT IO

SUB Main()
    PRINT "=== Model Compiler LLM — quantizzazione mista stile ds4 (olmoe) ==="
    MKDIR "work"

    IF NOT IO.FILEEXISTS("/Users/gaetano/ai/Models/gguf/olmoe-1b-7b-0924-instruct-q4_k_m.gguf") THEN
        PRINT "Modello assente: ~/ai/Models/gguf/olmoe-1b-7b-0924-instruct-q4_k_m.gguf"
        PRINT "Scaricalo (GGUF) in quella cartella e rilancia."
        END
    END IF

    ' ---- 1) ISPEZIONE: il piano elenca ruolo + formato proposto per ogni tensore ----
    LLM PLAN "/Users/gaetano/ai/Models/gguf/olmoe-1b-7b-0924-instruct-q4_k_m.gguf", "work/olmoe_piano.txt"
    PRINT "1) PLAN   -> work/olmoe_piano.txt"

    ' ---- 2) POLICY: la tabella "punto-di-decisione -> formato" (ricetta mista) ----
    '     NB: per il ROUTED_EXPERT il motore confronta il nome SENZA underscore
    '         ("iq2xxs", "q4k", "q2k", "q6k", "q5k", "q3k", "mxfp4", "nvfp4").
    LLM POLICY SET "ROUTED_EXPERT", "iq2xxs"    ' esperti MoE: ~2.06 bit
    LLM POLICY SET "FFN", "q4_0"                ' FFN densi
    LLM POLICY SET "ATTENTION", "q8_0"          ' proiezioni attention
    LLM POLICY SET "EMBEDDING", "f16"           ' embedding: nessuna perdita
    IF IO.FILEEXISTS("work/olmoe.imatrix.gguf") THEN
        LLM POLICY SET "IMATRIX", "work/olmoe.imatrix.gguf"
        PRINT "2) POLICY -> set (con imatrix)"
    ELSE
        PRINT "2) POLICY -> set (senza imatrix: euristica energetica del quantizer)"
    END IF
    PRINT "-- policy attive:"
    LLM POLICY SHOW

    ' ---- 3) POTA: 64 -> 32 esperti (stessa fetta sul router, *.expert_count aggiornato) ----
    ON ERROR GOTO E1
    LLM PRUNE "/Users/gaetano/ai/Models/gguf/olmoe-1b-7b-0924-instruct-q4_k_m.gguf", "work/olmoe_pruned.gguf", 32
    PRINT "3) PRUNE  -> work/olmoe_pruned.gguf  (64 -> 32 esperti)"
    E1:

    ' ---- 4) COMPRIMI: quantizza per RUOLO secondo la policy ----
    '     Prima il potato; se PRUNE non e' riuscito (file assente) ripiega sul sorgente.
    ON ERROR GOTO E2
    LLM QUANT "work/olmoe_pruned.gguf", "work/olmoe_compressed.gguf"
    PRINT "4) QUANT  -> work/olmoe_compressed.gguf  (dal potato)"
    GOTO FineQuant
    E2:
    ON ERROR GOTO E3
    LLM QUANT "/Users/gaetano/ai/Models/gguf/olmoe-1b-7b-0924-instruct-q4_k_m.gguf", "work/olmoe_compressed.gguf"
    PRINT "4) QUANT  -> work/olmoe_compressed.gguf  (dal sorgente)"
    GOTO FineQuant
    E3:
    PRINT "4) QUANT  -> ERRORE esplicito (vedi messaggio sopra)"
    END
    FineQuant:

    ' ---- 5) VERIFICA: ispeziona il RISULTATO (ruolo + formato di ogni tensore) ----
    LLM PLAN "work/olmoe_compressed.gguf", "work/olmoe_verify_piano.txt"
    PRINT "5) VERIFY -> work/olmoe_verify_piano.txt  (piano del modello compresso)"

    PRINT "compresso: work/olmoe_compressed.gguf"
    PRINT "quantize-olmoe: OK"
END SUB
