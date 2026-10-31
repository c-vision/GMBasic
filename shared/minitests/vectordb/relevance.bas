' RAG eval R2 — VECTORDB.RELEVANCE(domanda$, risposta$): quota token domanda presenti nella risposta.
' Domanda vuota o nessuna sovrapposizione → 0 (verdetto testuale).
' EXPECT: partial=  0.6|full=  0.75|none= zero-overlap|emptyq= zero-question
IMPORT VECTORDB

SUB Main()
    DIM q$ = "che cosa gestisce il motore"
    PRINT "partial= " + STR(MATH.ROUND(VECTORDB.RELEVANCE(q$, "il motore gestisce la memoria"), 4))
    PRINT "full= " + STR(MATH.ROUND(VECTORDB.RELEVANCE("che gestisce il motore", "il motore gestisce la memoria"), 4))
    IF VECTORDB.RELEVANCE(q$, "gmbasic compila basic") = 0 THEN
        PRINT "none= zero-overlap"
    ELSE
        PRINT "none= ERR"
    END IF
    IF VECTORDB.RELEVANCE("", "il motore gestisce") = 0 THEN
        PRINT "emptyq= zero-question"
    ELSE
        PRINT "emptyq= ERR"
    END IF
END SUB
