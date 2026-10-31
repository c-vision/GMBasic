' VECTORDB SQ8 (P1): storage quantizzato + scoring int8 con parità di recall vs FP32.
' Provider deterministico: fixture ESPLICITO (come indexmeta_filter_search.bas); :memory: fresca.
' Senza provider → parità di errore (-1/-1). Un solo EXPECT.
' EXPECT: sq8= ok
IMPORT AI
IMPORT VECTORDB

SUB Main()
    AI SET ACTIVE %EMBEDDING, "fixture-embedding"
    DIM idf = VECTORDB.ENSURECOLLECTION(":memory:", "cfp", 8)
    DIM ids = VECTORDB.ENSURECOLLECTION(":memory:", "csq", 8, "sq8")
    IF idf > 0 AND ids > 0 THEN
        VECTORDB.INDEXTEXT(":memory:", "cfp", "d0", "il motore gestisce la memoria gerarchica")
        VECTORDB.INDEXTEXT(":memory:", "cfp", "d1", "il compilatore emette codice nativo")
        VECTORDB.INDEXTEXT(":memory:", "cfp", "d2", "la rete neurale apprende dai dati")
        VECTORDB.INDEXTEXT(":memory:", "csq", "d0", "il motore gestisce la memoria gerarchica")
        VECTORDB.INDEXTEXT(":memory:", "csq", "d1", "il compilatore emette codice nativo")
        VECTORDB.INDEXTEXT(":memory:", "csq", "d2", "la rete neurale apprende dai dati")
        DIM nf = VECTORDB.COUNT(":memory:", "cfp")
        DIM nq = VECTORDB.COUNT(":memory:", "csq")
        DIM rf$ = VECTORDB.SEARCH(":memory:", "cfp", "memoria gerarchica", 1)
        DIM rq$ = VECTORDB.SEARCH(":memory:", "csq", "memoria gerarchica", 1)
        DIM tf$ = SourceOf(rf$)
        DIM tq$ = SourceOf(rq$)
        IF nf = 3 AND nq = 3 AND tf$ <> "" AND tf$ = tq$ THEN
            PRINT "sq8= ok"
        ELSE
            PRINT "sq8= MISMATCH"
        END IF
    ELSE
        IF idf = -1 AND ids = -1 THEN
            PRINT "sq8= ok"
        ELSE
            PRINT "sq8= ERR"
        END IF
    END IF
END SUB

' Prima occorrenza di "sourceId":"val" nel JSON di SEARCH ("" se assente).
FUNCTION SourceOf(r AS STRING) AS STRING
    DIM q$ = CHR(34)
    DIM k$ = q$ + "sourceId" + q$ + ":" + q$
    DIM p = INSTR(r, k$)
    IF p <= 0 THEN
        SourceOf = ""
    ELSE
        DIM start = p + 12
        DIM rest$ = MID(r, start, 200)
        DIM q2 = INSTR(rest$, q$)
        IF q2 <= 1 THEN
            SourceOf = ""
        ELSE
            SourceOf = MID(r, start, q2 - 1)
        END IF
    END IF
END FUNCTION
