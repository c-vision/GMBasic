' EXPECT: 2|0
' MiniTest — VectorDB SEARCH metadata-filter su IL (oracolo per la parità).
' Il filtro JSON sul tenant deve escludere il documento dell'altro tenant.
' Provider deterministico: fixture ESPLICITO (vedi indextext_outcomes.bas). :memory: fresca.
IMPORT AI

SUB Main()
    AI SET ACTIVE %EMBEDDING, "fixture-embedding"
    AI.INDEXMETA(":memory:", "ixf", "d1", "vacation policy allows 25 days",
        "{""status"":""current"",""tenant"":""acme""}")
    AI.INDEXMETA(":memory:", "ixf", "d2", "vacation policy allows 30 days",
        "{""status"":""current"",""tenant"":""globex""}")
    DIM hits AS STRING = AI.SEARCH(":memory:", "ixf", "vacation policy", 3, 1,
        "{""tenant"":""acme""}")
    DIM ok AS LONG = IIF(INSTR(hits, "d1") > 0, 2, -9)
    PRINT ok
    DIM no AS LONG = IIF(INSTR(hits, "d2") = 0, 0, -9)
    PRINT no
END SUB
