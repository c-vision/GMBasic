' Sample RAG Eval — VECTORDB.GENTEST(): generazione testset sintetici.
' Dimostra: ENSURECOLLECTION, INDEXTEXT, GENTEST con tutti i tipi di domanda.
' Richiede: provider embedding attivo (AI SET ACTIVE %EMBEDDING, ...)
' Richiede: provider chat attivo (per generazione domande via LLM)
' Fallback: senza LLM usa il testo del chunk come risposta.
IMPORT VECTORDB
IMPORT STRINGS

SUB Main()
    ' 1. Setup collection
    VECTORDB.ENSURECOLLECTION(":memory:", "sample_coll", 8)
    DIM count = VECTORDB.COUNT(":memory:", "sample_coll")
    IF count = 0 THEN
        PRINT "ERROR: embedding provider non configurato (serve AI SET ACTIVE %EMBEDDING)"
        END
    END IF

    ' 2. Index sample documents
    VECTORDB.INDEXTEXT(":memory:", "sample_coll", "doc1", "The Great Wall of China stretches over 13,000 miles. It was built over centuries to protect Chinese states.")
    VECTORDB.INDEXTEXT(":memory:", "sample_coll", "doc2", "Photosynthesis converts sunlight into chemical energy. Plants use chlorophyll to absorb light.")
    VECTORDB.INDEXTEXT(":memory:", "sample_coll", "doc3", "Albert Einstein developed the theory of relativity. E=mc^2 is his most famous equation.")
    VECTORDB.INDEXTEXT(":memory:", "sample_coll", "doc4", "The human brain contains approximately 86 billion neurons. It consumes about 20% of the body's energy.")

    ' 3. Count embeddings
    DIM emb_count = VECTORDB.COUNT(":memory:", "sample_coll")
    PRINT "Documenti indicizzati: " + STR$(emb_count)

    ' 4. Genera testset semplice (3 domande, tipo simple)
    DIM result1$ = VECTORDB.GENTEST("sample_coll", 3, "simple")
    PRINT "Testset simple: " + result1$

    ' 5. Genera testset multi-hop (2 domande)
    DIM result2$ = VECTORDB.GENTEST("sample_coll", 2, "multi-hop")
    PRINT "Testset multi-hop: " + result2$

    ' 6. Genera testset misto (simple + adversarial)
    DIM result3$ = VECTORDB.GENTEST("sample_coll", 4, "simple,adversarial")
    PRINT "Testset misto: " + result3$

    ' 7. Verifica file creato (parsing del path dal risultato)
    DIM path_start = INSTR(result1$, "samples/Fixtures/rag-eval/gentest_")
    IF path_start > 0 THEN
        PRINT "File salvato correttamente in samples/Fixtures/rag-eval/"
        ' Estrai il conteggio casi
        DIM count_pos = INSTR(result1$, "cases")
        IF count_pos > 0 THEN
            DIM count_val = VAL(MID$(result1$, count_pos - 3, 2))
            PRINT "Casi generati: " + STR$(count_val)
        END IF
    ELSE
        PRINT "Attenzione: posizione file non trovata nel risultato"
    END IF

    ' 8. Verifica sha256 presente
    DIM sha_pos = INSTR(result1$, "sha256=")
    IF sha_pos > 0 THEN
        PRINT "Hash SHA256 presente"
    ELSE
        PRINT "Hash SHA256 assente"
    END IF
END SUB