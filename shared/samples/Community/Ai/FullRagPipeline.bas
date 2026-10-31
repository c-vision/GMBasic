' FILE: AI/FullRagPipeline.bas
' =================================================================
' Pipeline RAG completa: ingest, search, rerank, ask
' NOTA PARITÀ: le gambe C usano vettori fixture deterministici SOLO con
' GMB_FIXTURE_EMBED=1 + provider embedding fixture attivo (opt-in esplicito,
' decisione 2026-09-18; default P0e: errore senza modello). Senza flag le gambe C
' danno errore esplicito mentre C# lavora con fixture (divergenza by-design).
' =================================================================
IMPORT SQL WITH ALIAS
IMPORT AI WITH ALIAS
IMPORT JSON

SUB Main()
    PRINT "Complete RAG Pipeline"
    PRINT "===================="
    PRINT

    ' --- 1. Setup ---
    AI.SETACTIVE("embedding", "fixture-embedding")
    AI.SETACTIVE("completion", "fixture-completion")
    SQL OPEN ":memory:" ALIAS ragdb
    AI RAG ENSURE COLLECTION "ragdb", "docs", 8
    PRINT "Database pronto."
    PRINT

    ' --- 2. Ingest ---
    AI RAG INDEX TEXT "ragdb", "docs", "src1", "Il machine learning e' un ramo dell AI."
    AI RAG INDEX TEXT "ragdb", "docs", "src2", "I transformer hanno rivoluzionato il NLP."
    AI RAG INDEX TEXT "ragdb", "docs", "src3", "Il deep learning usa reti neurali."
    PRINT "3 documenti indicizzati."
    PRINT

    ' --- 3. Search vettoriale ---
    DIM v$ = AI.SEARCH("ragdb", "docs", "machine learning", 3, 0)
    PRINT "Search vettoriale:"
    PRINT v$
    PRINT

    ' --- 4. Search ibrido ---
    DIM h$ = AI.SEARCH("ragdb", "docs", "reti neurali", 3, 1)
    PRINT "Search ibrido (FTS5+vector):"
    PRINT h$
    PRINT

    ' --- 5. Rerank ---
    DIM r$ = AI.RERANK("reti neurali profonde", h$, 3)
    PRINT "Dopo rerank:"
    PRINT r$
    PRINT

    ' --- 6. RAG context ---
    DIM ctx$ = AI.RAGCONTEXT("ragdb", "docs", "machine learning", 3)
    PRINT "RAG context:"
    PRINT ctx$
    PRINT

    ' --- 7. ASK ---
    DIM ans$ = AI.ASK("ragdb", "docs", "Cosa e' il ML?", 3)
    PRINT "ASK result: "; ans$
    PRINT

    ' --- 8. Statistiche ---
    PRINT "Embedding count: "; AI.COUNT("ragdb", "docs")
    PRINT "Collezioni:"
    PRINT AI.LISTCOLLECTIONS("ragdb")
    PRINT

    PRINT "Test completato."
END SUB
