' EXPECT: 0|2
' MiniTest — VectorDB INDEXMETA su IL (testo + metadata JSON, oracolo per la parità).
' Codici IL: 0=Indexed, 2=Updated (metadata cambiato = hash documento cambiato).
' Provider deterministico: fixture ESPLICITO (vedi indextext_outcomes.bas). :memory: fresca.
IMPORT AI

SUB Main()
    AI SET ACTIVE %EMBEDDING, "fixture-embedding"
    DIM n0 AS LONG = AI.INDEXMETA(":memory:", "ixm", "d1", "vacation policy allows 25 days",
        "{""status"":""current"",""tenant"":""acme""}")
    PRINT n0
    DIM n1 AS LONG = AI.INDEXMETA(":memory:", "ixm", "d1", "vacation policy allows 25 days",
        "{""status"":""deprecated"",""tenant"":""acme""}")
    PRINT n1
END SUB
