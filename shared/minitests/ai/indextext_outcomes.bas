' EXPECT: -1|-1|-1|-1
' MiniTest — VectorDB INDEXTEXT su IL (oracolo per la parità).
' DB persistente bin/vec.db (l'harness pulisce bin/ prima di ogni gamba).
IMPORT AI

SUB Main()
    AI SET ACTIVE %EMBEDDING, "fixture-embedding"
    DIM n0 AS LONG = AI.INDEXTEXT("bin/vec.db", "ix", "s1", "il gatto dorme sul divano")
    PRINT n0
    DIM n1 AS LONG = AI.INDEXTEXT("bin/vec.db", "ix", "s2", "il cane corre nel parco")
    PRINT n1
    DIM n2 AS LONG = AI.INDEXTEXT("bin/vec.db", "ix", "s1", "il gatto dorme sul divano")
    PRINT n2
    DIM n3 AS LONG = AI.INDEXTEXT("bin/vec.db", "ix", "s1", "il gatto dorme sul divano, russo")
    PRINT n3
END SUB
