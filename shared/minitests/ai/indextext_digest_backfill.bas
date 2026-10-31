' EXPECT: 0|0
' MiniTest — VectorDB INDEXTEXT con digest: re-index stesso provider = idempotente.
' Primo accesso a una collection SENZA digest (simula DB pre-migrazione) → backfill,
' quindi niente falso rifiuto.
' Provider deterministico: fixture ESPLICITO (default registrato da EnsureDefaultsLocked;
' niente AI REGISTER, che è alias-first e rifiuta uno scheme nudo come "fixture").
' :memory: = connessione fresca (persistente nel run, parità col runtime C).
IMPORT AI

SUB Main()
    AI SET ACTIVE %EMBEDDING, "fixture-embedding"
    AI.INDEXTEXT(":memory:", "ixd", "s1", "hello world")
    PRINT 0
    AI.INDEXTEXT(":memory:", "ixd", "s1", "hello world")
    PRINT 0
END SUB
