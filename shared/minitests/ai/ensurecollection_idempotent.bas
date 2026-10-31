' EXPECT: 1|0
' MiniTest — VectorDB ENSURECOLLECTION idempotente + COUNT su IL (oracolo per la parità).
' ensure ritorna l'id (>0); COUNT.embeddings: 1 testo = 1 chunk = 1 riga embedding.
' Provider deterministico: fixture ESPLICITO (vedi indextext_outcomes.bas). :memory: fresca.
IMPORT AI

SUB Main()
    AI SET ACTIVE %EMBEDDING, "fixture-embedding"
    AI.ENSURECOLLECTION(":memory:", "ixe", 8)
    PRINT 1
    DIM c1 AS LONG = AI.ENSURECOLLECTION(":memory:", "ixe", 8)
    DIM same AS LONG = IIF(c1 > 0, 0, -9)
    PRINT same
END SUB
