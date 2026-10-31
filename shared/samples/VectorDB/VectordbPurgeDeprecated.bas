' FILE: VectorDB/VectordbPurgeDeprecated.bas
' =================================================================================================
' VECTORDB.PURGEDEPRECATED(db$, coll$) — rimuove documenti deprecati da una collection vettoriale
' NOTA: richiede un database vettoriale esistente con documents caricati.
' =================================================================================================
IMPORT VECTORDB

SUB Main()
    CLS
    PRINT "=== VECTORDB.PURGEDEPRECATED ==="
    DIM result = VECTORDB.PURGEDEPRECATED("vectordb_test", "documenti")
    PRINT "Purged entries: "; result
    PRINT "=== OK ==="
END SUB