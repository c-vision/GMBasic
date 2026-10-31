' RagEvalMetrics.bas — RAG eval end-to-end (todo Rag-Eval-Gap-Analysis, Fase A: R1+R2).
' Scenario: golden set con 2 query; per ognuna misuriamo il quartetto retrieval
' (P@K/R@K/MRR/NDCG) e giudichiamo due risposte candidate (fedele vs parziale).
' Deterministico: nessuna rete, nessun modello, nessun file in work/.
' NB: le righe che possono stampare ZERO usano verdetti testuali — la gamba VM/C formatta
' STR(0) senza il padding del segno (divergenza pre-esistente del formatter, fuori scope).
IMPORT VECTORDB

FUNCTION Verdict(score) AS STRING
    IF score >= 0.8 THEN
        Verdict = "OK"
    ELSEIF score >= 0.5 THEN
        Verdict = "MEH"
    ELSE
        Verdict = "BAD"
    END IF
END FUNCTION

SUB Main()
    ' ── R1: quartetto retrieval su due query ─────────────────────────────
    DIM r1$ = "d1,d2,d3,d4"
    DIM i1$ = "d1,d2"
    PRINT "q1 p@2= " + STR(MATH.ROUND(VECTORDB.PRECISIONAT(r1$, i1$, 2), 4))
    PRINT "q1 r@2= " + STR(MATH.ROUND(VECTORDB.RECALLAT(r1$, i1$, 2), 4))
    PRINT "q1 mrr= " + STR(MATH.ROUND(VECTORDB.MRR(r1$, i1$), 4))
    PRINT "q1 ndcg@2= " + STR(MATH.ROUND(VECTORDB.NDCGAT(r1$, i1$, 2), 4))

    DIM r2$ = "d3,d1,d4,d2"
    DIM i2$ = "d1,d2"
    PRINT "q2 p@2= " + STR(MATH.ROUND(VECTORDB.PRECISIONAT(r2$, i2$, 2), 4))
    PRINT "q2 r@2= " + STR(MATH.ROUND(VECTORDB.RECALLAT(r2$, i2$, 2), 4))
    PRINT "q2 mrr= " + STR(MATH.ROUND(VECTORDB.MRR(r2$, i2$), 4))
    PRINT "q2 ndcg@2= " + STR(MATH.ROUND(VECTORDB.NDCGAT(r2$, i2$, 2), 4))

    ' ── R2: giudizi generazione tier-1 (euristiche deterministiche) ─────
    DIM ctx$ = "il motore mlx gestisce la memoria kv con retenzione su apple silicon"
    DIM buona$ = "il motore mlx gestisce la memoria kv"
    DIM povera$ = "gmbasic compila basic in eseguibili nativi"

    PRINT "buona faithfulness= " + STR(MATH.ROUND(VECTORDB.FAITHFULNESS(ctx$, buona$), 4))
    PRINT "buona relevance= " + STR(MATH.ROUND(VECTORDB.RELEVANCE("che cosa gestisce il motore", buona$), 4))
    PRINT "buona completeness= " + STR(MATH.ROUND(VECTORDB.COMPLETENESS("che cosa gestisce il motore", buona$), 4))
    IF VECTORDB.HALLUCINATION(ctx$, buona$) = 0 THEN
        PRINT "buona hallucination= zero (risposta interamente radicata)"
    ELSE
        PRINT "buona hallucination= " + STR(MATH.ROUND(VECTORDB.HALLUCINATION(ctx$, buona$), 4))
    END IF
    IF VECTORDB.FAITHFULNESS(ctx$, povera$) = 0 THEN
        PRINT "povera faithfulness= zero (nessuna affermazione radicata)"
    ELSE
        PRINT "povera faithfulness= " + STR(MATH.ROUND(VECTORDB.FAITHFULNESS(ctx$, povera$), 4))
    END IF
    IF VECTORDB.HALLUCINATION(ctx$, povera$) = 1 THEN
        PRINT "povera hallucination= total (risposta estranea al contesto)"
    ELSE
        PRINT "povera hallucination= " + STR(MATH.ROUND(VECTORDB.HALLUCINATION(ctx$, povera$), 4))
    END IF
    IF VECTORDB.RELEVANCE("che cosa gestisce il motore", povera$) = 0 THEN
        PRINT "povera relevance= zero (non risponde alla domanda)"
    ELSE
        PRINT "povera relevance= " + STR(MATH.ROUND(VECTORDB.RELEVANCE("che cosa gestisce il motore", povera$), 4))
    END IF

    ' ── Verdetto: la risposta fedele supera la soglia, la povera no ─────
    PRINT "verdetto buona= " + Verdict(VECTORDB.FAITHFULNESS(ctx$, buona$))
    PRINT "verdetto povera= " + Verdict(VECTORDB.RELEVANCE("che cosa gestisce il motore", povera$))
END SUB
