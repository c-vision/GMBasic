' EXPECT: 2|0|purged
' MiniTest — forme STATEMENT di AI RAG: ENSURE COLLECTION, INDEX META, PURGE DEPRECATED.
' Superficie risolta dal dispatch C99Native (gmb_c99_emit.c) e dal pass-through del
' filter di AI.SEARCH su emitter C# (gmb_rag_search_sel, arità completa).
' Provider deterministico fixture ESPLICITO; :memory: fresca. Oracolo: il filtro tenant
' deve includere d1 (acme) ed escludere d2 (globex); PURGE DEPRECATED non deve crashare.
IMPORT AI

SUB Main()
    AI SET ACTIVE %EMBEDDING, "fixture-embedding"
    AI RAG ENSURE COLLECTION ":memory:", "stmts", 8
    AI RAG INDEX META ":memory:", "stmts", "d1", "vacation policy allows 25 days", "{""tenant"":""acme""}"
    AI RAG INDEX META ":memory:", "stmts", "d2", "vacation policy allows 30 days", "{""tenant"":""globex""}"
    DIM hits AS STRING = AI.SEARCH(":memory:", "stmts", "vacation policy", 3, 1, "{""tenant"":""acme""}")
    PRINT IIF(INSTR(hits, "d1") > 0, 2, -9)
    PRINT IIF(INSTR(hits, "d2") = 0, 0, -9)
    AI RAG PURGE DEPRECATED ":memory:", "stmts"
    PRINT "purged"
END SUB
