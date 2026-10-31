' FILE: AI/ProviderReranker.bas
' =================================================================
' Reranker come terzo ruolo PROVIDER (embedding | completion | reranker):
' priorita', stato e health-check si applicano automaticamente anche qui.
' =================================================================
IMPORT AI WITH ALIAS
IMPORT SQL WITH ALIAS

SUB Main()
    PRINT "Provider Reranker Test"
    PRINT "======================="
    PRINT

    ' --- 1. Il reranker di default (keyword-reranker) esiste gia', zero-config ---
    PRINT "Reranker attivo di default: "; AI.ACTIVE("reranker")
    PRINT

    ' --- 2. Un dataset di prova e ricerca vettoriale ---
    SQL OPEN ":memory:" ALIAS rerankdb
    AI RAG ENSURE COLLECTION "rerankdb", "docs", 8
    AI RAG INDEX TEXT "rerankdb", "docs", "d1", "Il machine learning e' un ramo dell' AI."
    AI RAG INDEX TEXT "rerankdb", "docs", "d2", "I transformer hanno rivoluzionato il NLP."
    AI RAG INDEX TEXT "rerankdb", "docs", "d3", "Il deep learning usa reti neurali profonde."

    DIM hits$ = AI.SEARCH("rerankdb", "docs", "reti neurali profonde", 3, TRUE, "")
    PRINT "Risultati SEARCH (ordine originale):"
    PRINT hits$
    PRINT

    DIM reranked$ = AI.RERANK("reti neurali profonde", hits$, 3)
    PRINT "Risultati dopo RERANK (keyword-reranker):"
    PRINT reranked$
    PRINT

    ' --- 3. Registra un secondo reranker con priorita' piu' alta ---
    AI.REGISTER("reranker", "custom-reranker", "keyword", """{}""")
    AI.SETPRIORITY("custom-reranker", 100)
    PRINT "Reranker attivo dopo registrazione (priorita' piu' alta vince): "; AI.ACTIVE("reranker")
    PRINT

    ' --- 4. Bloccalo: il reranker di default torna attivo ---
    AI.SETSTATUS("custom-reranker", "BLOCKED")
    PRINT "Reranker attivo dopo blocco: "; AI.ACTIVE("reranker")
    PRINT

    ' --- 5. Test di connettivita' (funziona per qualunque kind, keyword incluso) ---
    PRINT "--- AI.TEST ---"
    PRINT AI.TEST("reranker", "keyword-reranker")

    PRINT
    PRINT "Test Completato."
END SUB
