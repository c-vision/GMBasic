' RAG eval R1 — VECTORDB.RECALLAT(retrieved$, ideal$, k): hit nei primi k / |ideal| (k=0: tutta la lista).
' Handbook: lista "a,b,c,d", ideali "b,d". ideal$ vuota → 0.
' EXPECT: r2=  0.5|r4=  1|all=  1|miss= half|noideal= zero-ideal
IMPORT VECTORDB

SUB Main()
    DIM r$ = "a,b,c,d"
    DIM i$ = "b,d"
    PRINT "r2= " + STR(MATH.ROUND(VECTORDB.RECALLAT(r$, i$, 2), 4))
    PRINT "r4= " + STR(MATH.ROUND(VECTORDB.RECALLAT(r$, i$, 4), 4))
    PRINT "all= " + STR(MATH.ROUND(VECTORDB.RECALLAT(r$, i$, 0), 4))
    IF VECTORDB.RECALLAT(r$, "b,z", 4) = 0.5 THEN
        PRINT "miss= half"
    ELSE
        PRINT "miss= ERR"
    END IF
    IF VECTORDB.RECALLAT(r$, "", 4) = 0 THEN
        PRINT "noideal= zero-ideal"
    ELSE
        PRINT "noideal= ERR"
    END IF
END SUB
