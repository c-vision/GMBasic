' RAG eval (todo Rag-Eval-Gap-Analysis Fase A): quartetto retrieval (R1) + giudizi generazione tier-1 (R2).
' Oracolo = valori handbook per costruzione (lista "a,b,c,d", ideali "b,d", contesti/domande toy).
' Se una gamba diverge qui, la tokenizzazione/le formule C# vs C non sono paritarie: NON riconciliare a occhio.
' EXPECT: p2=  0.5|p4=  0.5|r2=  0.5|rall=  1|mrr=  0.5|mrrno= no-hit|ndcg1=  1|ndcg2=  0.6509|fai=  0.75|hal=  0.25|rel=  0.6|com=  0.5
' NB: mrrno come verdetto testuale — la gamba VM/C formatta STR(0) senza il padding del segno
' (divergenza pre-esistente del formatter, fuori scope qui: vedi session log 2026-09-14).
IMPORT VECTORDB

SUB Main()
    DIM r$ = "a,b,c,d"
    DIM i$ = "b,d"
    PRINT "p2= " + STR(MATH.ROUND(VECTORDB.PRECISIONAT(r$, i$, 2), 4))
    PRINT "p4= " + STR(MATH.ROUND(VECTORDB.PRECISIONAT(r$, i$, 4), 4))
    PRINT "r2= " + STR(MATH.ROUND(VECTORDB.RECALLAT(r$, i$, 2), 4))
    PRINT "rall= " + STR(MATH.ROUND(VECTORDB.RECALLAT(r$, i$, 0), 4))
    PRINT "mrr= " + STR(MATH.ROUND(VECTORDB.MRR(r$, i$), 4))
    IF VECTORDB.MRR("x,y", i$) = 0 THEN PRINT "mrrno= no-hit" ELSE PRINT "mrrno= ERR"
    PRINT "ndcg1= " + STR(MATH.ROUND(VECTORDB.NDCGAT("b,d,a,c", i$, 4), 4))
    PRINT "ndcg2= " + STR(MATH.ROUND(VECTORDB.NDCGAT(r$, i$, 4), 4))
    PRINT "fai= " + STR(MATH.ROUND(VECTORDB.FAITHFULNESS("il motore mlx gestisce la memoria", "motore mlx memoria rapido"), 4))
    PRINT "hal= " + STR(MATH.ROUND(VECTORDB.HALLUCINATION("il motore mlx gestisce la memoria", "motore mlx memoria rapido"), 4))
    PRINT "rel= " + STR(MATH.ROUND(VECTORDB.RELEVANCE("che cosa gestisce il motore", "il motore gestisce la memoria"), 4))
    PRINT "com= " + STR(MATH.ROUND(VECTORDB.COMPLETENESS("che cosa gestisce il motore", "il motore gestisce la memoria"), 4))
END SUB
