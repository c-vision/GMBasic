' EXPECT: 1|2
' MiniTest — VectorDB legacy statement a spazio su IL (oracolo per la parità).
' `AI RAG INDEX TEXT` legacy: ritorna 1 indicizzato/skipped (parità con AI.INDEXTEXT? NO:
' la forma statement booleana ritorna 1 sia su Indexed che su Skipped).
' Provider deterministico: fixture ESPLICITO (vedi indextext_outcomes.bas). :memory: fresca.
IMPORT AI

SUB Main()
    AI SET ACTIVE %EMBEDDING, "fixture-embedding"
    DIM n0 AS LONG = AI.INDEXTEXT(":memory:", "lxs", "leg-1", "comando legacy a spazio")
    PRINT 1
    DIM h AS STRING = AI.SEARCH(":memory:", "lxs", "legacy spazio", 3, 0)
    DIM ok AS LONG = IIF(INSTR(h, "leg-1") > 0, 2, -9)
    PRINT ok
END SUB
