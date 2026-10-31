' FILE: Databases/DbSqlUncoveredCoverage.bas
' =================================================================================================
' TEST UNITARIO: SQL - FUNZIONI NON ANCORA COPERTE DAGLI ESEMPI
' =================================================================================================
' Questo test copre le funzioni SQL che non comparivano in alcun esempio .bas:
'   LASTINSERTROWID, CHANGES.
'
' COPERTURA:
' - SQL.LASTINSERTROWID(db) : ultimo rowid -> LONG.
' - SQL.CHANGES(db)         : righe modificate -> LONG.
'
' NOTA D'USO: il driver SQLite usa l'ALIAS come nome di tabella per LoadTableData
' durante l'apertura; quindi si crea una tabella con lo stesso nome dell'ALIAS.
' =================================================================================================
IMPORT SQL WITH ALIAS
IMPORT IO WITH ALIAS

SUB Main()
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work
    CLS
    PRINT "============================================================"
    PRINT "SQL - Funzioni scoperte dagli esempi"
    PRINT "============================================================"
    PRINT

    MKDIR g_work
    KILL g_work + "/uncovered_sql.db": ON ERROR RESUME NEXT

    ' Apre una base nuova e crea la tabella con lo stesso nome dell'ALIAS.
    SQL OPEN g_work + "/uncovered_sql.db" ALIAS SqlDb
    SQL EXEC SqlDb, "CREATE TABLE IF NOT EXISTS SqlDb (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT)"
    SQL EXEC SqlDb, "DELETE FROM SqlDb"
    SQL EXEC SqlDb, "INSERT INTO SqlDb (name) VALUES ('prodotto')"

    ' 1. LASTINSERTROWID
    PRINT "-- LASTINSERTROWID --"
    DIM lastId AS LONG
    lastId = SQL.LASTINSERTROWID("SqlDb")
    PRINT "LASTINSERTROWID(SqlDb) = "; lastId
    PRINT

    ' 2. CHANGES
    PRINT "-- CHANGES --"
    DIM nChanged AS LONG
    ' MODIFICA una riga per produrre un conteggio di CHANGES.
    SQL EXEC SqlDb, "UPDATE SqlDb SET name = 'prodotto2' WHERE name = 'prodotto'"
    nChanged = SQL.CHANGES("SqlDb")
    PRINT "CHANGES(SqlDb) = "; nChanged; " (righe modificate)."
    PRINT

    SQL CLOSE SqlDb
    PRINT "Test Completato."
END SUB
