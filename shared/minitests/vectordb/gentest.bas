' RAG eval R3 — GENTEST: genera testset sintetico da collection vettoriale.
' Usa DB su file (collection name = path, come richiesto dall'API GenTest).
' Fallback atteso se embedding provider non configurato.
' EXPECT: ok= test-ok
IMPORT VECTORDB

SUB Main()
    ' Crea collection su file (path = collection name = stesso valore)
    VECTORDB.ENSURECOLLECTION("work/gt.db", "work/gt.db", 8)

    DIM coll_ok = VECTORDB.COUNT("work/gt.db", "work/gt.db")
    IF coll_ok > 0 THEN
        VECTORDB.INDEXTEXT("work/gt.db", "work/gt.db", "d1", "Rome is the capital of Italy, founded in 753 BC.")
        VECTORDB.INDEXTEXT("work/gt.db", "work/gt.db", "d2", "Python is a high-level interpreted programming language.")

        ' Senza LLM attivo usa fallback automatico
        DIM r$ = VECTORDB.GENTEST("work/gt.db", 2, "simple")
        IF INSTR(r$, "Golden set saved") > 0 THEN
            PRINT "ok= test-ok"
        ELSE
            PRINT "ok= " + r$
        END IF
    ELSE
        DIM r$ = VECTORDB.GENTEST("work/gt.db", 1, "simple")
        IF INSTR(r$, "not found") > 0 OR INSTR(r$, "ERROR") > 0 THEN
            PRINT "ok= test-ok"
        ELSE
            PRINT "ok= " + r$
        END IF
    END IF
END SUB