' VECTORDB SQ8 — demo completa: due collezioni gemelle (FP32 vs SQ8), index con metadati,
' COUNT, SEARCH vettoriale + hybrid, LISTCOLLECTIONS, REMOVESOURCE, parità di recall.
' Provider deterministico: fixture ESPLICITO; :memory: fresca (nessun file scritto).
IMPORT AI
IMPORT VECTORDB

SUB Main()
    AI SET ACTIVE %EMBEDDING, "fixture-embedding"
    DIM idf = VECTORDB.ENSURECOLLECTION("work/vsq8fp.db", "docs", 8)
    DIM ids = VECTORDB.ENSURECOLLECTION("work/vsq8q.db", "docs", 8, "sq8")
    IF idf <= 0 OR ids <= 0 THEN
        PRINT "VectordbSq8: provider embedding non attivo, demo non eseguibile qui."
    ELSE
        VECTORDB.INDEXTEXT("work/vsq8fp.db", "docs", "motore", "il motore gestisce la memoria gerarchica")
        VECTORDB.INDEXTEXT("work/vsq8fp.db", "docs", "compilatore", "il compilatore emette codice nativo")
        VECTORDB.INDEXMETA("work/vsq8fp.db", "docs", "rete", "la rete neurale apprende dai dati", "{""tema"":""ml""}")
        VECTORDB.INDEXTEXT("work/vsq8q.db", "docs", "motore", "il motore gestisce la memoria gerarchica")
        VECTORDB.INDEXTEXT("work/vsq8q.db", "docs", "compilatore", "il compilatore emette codice nativo")
        VECTORDB.INDEXMETA("work/vsq8q.db", "docs", "rete", "la rete neurale apprende dai dati", "{""tema"":""ml""}")
        PRINT "count fp32="; VECTORDB.COUNT("work/vsq8fp.db", "docs"); " sq8="; VECTORDB.COUNT("work/vsq8q.db", "docs")
        PRINT "collections="; VECTORDB.LISTCOLLECTIONS("work/vsq8fp.db")
        DIM v1$ = VECTORDB.SEARCH("work/vsq8fp.db", "docs", "memoria gerarchica", 2, 0, "")
        DIM v2$ = VECTORDB.SEARCH("work/vsq8q.db", "docs", "memoria gerarchica", 2, 0, "")
        PRINT "vector fp32 top: "; TopSource(v1$)
        PRINT "vector sq8  top: "; TopSource(v2$)
        DIM h1$ = VECTORDB.SEARCH("work/vsq8fp.db", "docs", "memoria gerarchica", 2, 1, "")
        DIM h2$ = VECTORDB.SEARCH("work/vsq8q.db", "docs", "memoria gerarchica", 2, 1, "")
        PRINT "hybrid fp32 top: "; TopSource(h1$)
        PRINT "hybrid sq8  top: "; TopSource(h2$)
        IF TopSource(v1$) = TopSource(v2$) AND TopSource(h1$) = TopSource(h2$) THEN
            PRINT "Parita di recall (vector+hybrid): OK"
        ELSE
            PRINT "Parita di recall: MISMATCH"
        END IF
        VECTORDB.REMOVESOURCE("work/vsq8fp.db", "docs", "rete")
        VECTORDB.REMOVESOURCE("work/vsq8q.db", "docs", "rete")
        PRINT "dopo remove count fp32="; VECTORDB.COUNT("work/vsq8fp.db", "docs"); " sq8="; VECTORDB.COUNT("work/vsq8q.db", "docs")
    END IF
END SUB

' Prima occorrenza di "sourceId":"val" nel JSON di SEARCH ("" se assente).
FUNCTION TopSource(r AS STRING) AS STRING
    DIM q$ = CHR(34)
    DIM k$ = q$ + "sourceId" + q$ + ":" + q$
    DIM p = INSTR(r, k$)
    IF p <= 0 THEN
        TopSource = ""
    ELSE
        DIM start = p + 12
        DIM rest$ = MID(r, start, 200)
        DIM q2 = INSTR(rest$, q$)
        IF q2 <= 1 THEN
            TopSource = ""
        ELSE
            TopSource = MID(r, start, q2 - 1)
        END IF
    END IF
END FUNCTION
