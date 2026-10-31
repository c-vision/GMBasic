' EXPECT: 1|1
' MiniTest — VectorDB REMOVESOURCE + PURGEDEPRECATED su IL (oracolo per la parità).
' REMOVESOURCE ritorna 1.0; PURGEDEPRECATED conta i deprecated eliminati (1), niente residui.
' emitter C#: DELETETEXT e PURGEDEPRECATED emettono il vero runtime C (parità).
' Provider deterministico: fixture ESPLICITO (vedi indextext_outcomes.bas). :memory: fresca.
IMPORT AI

SUB Main()
    AI SET ACTIVE %EMBEDDING, "fixture-embedding"
    AI.INDEXMETA(":memory:", "ixr", "d1", "temporary document to remove",
        "{""status"":""current""}")
    AI.INDEXMETA(":memory:", "ixr", "d2", "obsolete document",
        "{""status"":""deprecated""}")
    DIM r0 AS LONG = AI.REMOVESOURCE(":memory:", "ixr", "d1")
    PRINT r0
    DIM p0 AS LONG = AI.PURGEDEPRECATED(":memory:", "ixr")
    PRINT p0
END SUB
