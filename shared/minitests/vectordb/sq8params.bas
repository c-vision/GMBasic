' VECTORDB SQ8 — parità parametri di ENSURECOLLECTION sui 4 backend.
' Senza provider embedding ogni forma ritorna -1 (parità di errore IL/PCode/VM-C/C99).
' Formati non validi ("xyz", "SQ8" maiuscolo) non devono crashare: trattati come fp32/keep.
' EXPECT: 1|1|1|1|1
IMPORT VECTORDB

SUB Main()
    DIM e2 = VECTORDB.ENSURECOLLECTION("work/sq8p.db", "c")
    DIM e3 = VECTORDB.ENSURECOLLECTION("work/sq8p.db", "c", 8)
    DIM e4 = VECTORDB.ENSURECOLLECTION("work/sq8p.db", "c", 8, "sq8")
    DIM ex = VECTORDB.ENSURECOLLECTION("work/sq8p.db", "c", 8, "xyz")
    DIM eu = VECTORDB.ENSURECOLLECTION("work/sq8p.db", "c", 8, "SQ8")
    PRINT IIF(e2 = -1, 1, 0)
    PRINT IIF(e3 = -1, 1, 0)
    PRINT IIF(e4 = -1, 1, 0)
    PRINT IIF(ex = -1, 1, 0)
    PRINT IIF(eu = -1, 1, 0)
END SUB
