' RAG eval R1 — VECTORDB.MRR(retrieved$, ideal$): 1/rango del primo hit (nessun taglio a K).
' Handbook: "a,b,c,d" vs "b,d" → primo hit a rango 2 → 0.5. Nessun hit → 0 (verdetto testuale).
' EXPECT: rank1=  1|rank2=  0.5|rank3=  0.3333|nohit= no-hit
IMPORT VECTORDB

SUB Main()
    DIM i$ = "b,d"
    PRINT "rank1= " + STR(MATH.ROUND(VECTORDB.MRR("b,a,c,d", i$), 4))
    PRINT "rank2= " + STR(MATH.ROUND(VECTORDB.MRR("a,b,c,d", i$), 4))
    PRINT "rank3= " + STR(MATH.ROUND(VECTORDB.MRR("a,c,b,d", i$), 4))
    IF VECTORDB.MRR("x,y,z", i$) = 0 THEN
        PRINT "nohit= no-hit"
    ELSE
        PRINT "nohit= ERR"
    END IF
END SUB
