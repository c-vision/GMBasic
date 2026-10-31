' RAG eval R1 — VECTORDB.PRECISIONAT(retrieved$, ideal$, k): hit nei primi k / k.
' Handbook: lista "a,b,c,d", ideali "b,d". k<=0 → 0 (per progetto); retrieved vuota → 0.
' EXPECT: p2=  0.5|p4=  0.5|full=  1|k0= zero-k|empty= no-retrieved
IMPORT VECTORDB

SUB Main()
    DIM r$ = "a,b,c,d"
    DIM i$ = "b,d"
    PRINT "p2= " + STR(MATH.ROUND(VECTORDB.PRECISIONAT(r$, i$, 2), 4))
    PRINT "p4= " + STR(MATH.ROUND(VECTORDB.PRECISIONAT(r$, i$, 4), 4))
    PRINT "full= " + STR(MATH.ROUND(VECTORDB.PRECISIONAT("d,b,a,c", i$, 2), 4))
    IF VECTORDB.PRECISIONAT(r$, i$, 0) = 0 THEN
        PRINT "k0= zero-k"
    ELSE
        PRINT "k0= ERR"
    END IF
    IF VECTORDB.PRECISIONAT("", i$, 2) = 0 THEN
        PRINT "empty= no-retrieved"
    ELSE
        PRINT "empty= ERR"
    END IF
END SUB
