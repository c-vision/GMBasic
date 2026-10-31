' RAG eval R1 — VECTORDB.NDCGAT(retrieved$, ideal$, k): DCG/IDCG binario, sconto log2(i+2).
' Handbook: ordine perfetto = 1 (esatto); parziale "a,b,c,d" vs "b,d" @4 = 0.6509 (tolleranza 1e-6);
' nessun hit nei primi k → 0 (verdetto).
' EXPECT: perfect=  1|partial=  0.6509|worst= zero-dcg|empty= zero-list
IMPORT VECTORDB

SUB Main()
    DIM i$ = "b,d"
    PRINT "perfect= " + STR(MATH.ROUND(VECTORDB.NDCGAT("b,d,a,c", i$, 4), 4))
    PRINT "partial= " + STR(MATH.ROUND(VECTORDB.NDCGAT("a,b,c,d", i$, 4), 4))
    IF VECTORDB.NDCGAT("x,y,z,w", i$, 4) = 0 THEN
        PRINT "worst= zero-dcg"
    ELSE
        PRINT "worst= ERR"
    END IF
    IF VECTORDB.NDCGAT("", i$, 4) = 0 THEN
        PRINT "empty= zero-list"
    ELSE
        PRINT "empty= ERR"
    END IF
END SUB
