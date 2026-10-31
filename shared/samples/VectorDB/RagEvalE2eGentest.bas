' RAG eval R3 — GENTEST end-to-end con LLM locale (Llama-3.2-1B).
' Registra provider MLX embedding e chat, popola collection, genera testset.
IMPORT VECTORDB
IMPORT STRINGS
IMPORT AI WITH ALIAS
IMPORT LLM

SUB Main()
    ' Verifica modello locale presente
    IF NOT IO.DIREXISTS("/Users/gaetano/ai/Models/Llama-3.2-1B-Instruct-4bit") THEN
        PRINT "Modello locale assente. Percorso atteso:"
        PRINT "/Users/gaetano/ai/Models/Llama-3.2-1B-Instruct-4bit"
        END
    END IF

    ' 1. Registra provider embedding (latent hidden states dal LLM)
    AI REGISTER "llama1b-emb", %EMBEDDING, "mlx", "{""modelDir"":""/Users/gaetano/ai/Models/Llama-3.2-1B-Instruct-4bit"",""maxTokens"":""8""}"
    AI SET ACTIVE %EMBEDDING, "llama1b-emb"
    PRINT "Embedding provider attivato: "; AI.ACTIVE(%EMBEDDING)

    ' 2. Registra provider chat (completion con Llama-1B, deterministica)
    AI REGISTER "llama1b", %CHAT, "/Users/gaetano/ai/Models/Llama-3.2-1B-Instruct-4bit", "{""maxTokens"":""64"",""temperature"":""0""}"
    AI SET ACTIVE %CHAT, "llama1b"
    PRINT "Chat provider attivato: "; AI.ACTIVE(%CHAT)

    ' 3. Crea collection e indicizza documenti
    VECTORDB.ENSURECOLLECTION(":memory:", "gentest_docs", 8)

    VECTORDB.INDEXTEXT(":memory:", "gentest_docs", "d1", "Rome is the capital city of Italy. It was founded in 753 BC.")
    VECTORDB.INDEXTEXT(":memory:", "gentest_docs", "d2", "Python is a high-level programming language created by Guido van Rossum.")
    VECTORDB.INDEXTEXT(":memory:", "gentest_docs", "d3", "The Amazon rainforest is the largest tropical rainforest in the world.")
    VECTORDB.INDEXTEXT(":memory:", "gentest_docs", "d4", "Albert Einstein developed the theory of relativity. E=mc^2 describes energy-mass equivalence.")
    VECTORDB.INDEXTEXT(":memory:", "gentest_docs", "d5", "Photosynthesis is the process by which plants convert sunlight into chemical energy.")

    DIM emb_count = VECTORDB.COUNT(":memory:", "gentest_docs")
    PRINT "Documenti indicizzati: " + STR$(emb_count)
    IF emb_count = 0 THEN
        PRINT "ERRORE: nessun documento indicizzato (provider non funzionante?)"
        END
    END IF

    ' 4. GENTEST — genera testset sintetico (3 domande simple)
    PRINT "Generazione testset in corso..."
    DIM result$ = VECTORDB.GENTEST("gentest_docs", 3, "simple")
    PRINT "Risultato:"
    PRINT result$

    ' 5. Verifica file creato
    DIM pos = INSTR(result$, "samples/Fixtures/rag-eval/gentest_")
    IF pos > 0 THEN
        DIM path$ = MID$(result$, pos)
        DIM end_pos = INSTR(path$, " (")
        IF end_pos > 0 THEN path$ = LEFT$(path$, end_pos - 1)
        PRINT "File: " + path$
        DIM cpos = INSTR(result$, "cases")
        IF cpos > 0 THEN PRINT "Casi: " + MID$(result$, cpos - 3, 3)
        DIM spos = INSTR(result$, "sha256=")
        IF spos > 0 THEN PRINT "SHA256: " + MID$(result$, spos + 7)
    ELSE
        PRINT "ERRORE: file non creato - " + result$
    END IF

    PRINT ""
    PRINT "=== GENTEST E2E COMPLETATO ==="
END SUB