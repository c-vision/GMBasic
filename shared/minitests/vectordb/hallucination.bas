' RAG eval R2 — VECTORDB.HALLUCINATION(contesto$, risposta$) = 1 - FAITHFULNESS.
' Risposta estranea al contesto → 1; risposta vuota → 1 per formula (complemento di 0).
' EXPECT: partial=  0.25|stranger=  1|empty=  1
IMPORT VECTORDB

SUB Main()
    DIM ctx$ = "il motore mlx gestisce la memoria"
    PRINT "partial= " + STR(MATH.ROUND(VECTORDB.HALLUCINATION(ctx$, "motore mlx memoria rapido"), 4))
    PRINT "stranger= " + STR(MATH.ROUND(VECTORDB.HALLUCINATION(ctx$, "gmbasic compila basic"), 4))
    PRINT "empty= " + STR(MATH.ROUND(VECTORDB.HALLUCINATION(ctx$, ""), 4))
END SUB
