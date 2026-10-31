' RDD_SQL_CRUD.bas — SQL RDD con CRUD completo
' ===================================================================
' Apre un database SQLite in memoria, crea tabella con SQL nativo,
' poi opera via RDD. Dimostra INSERT/UPDATE/DELETE/SELECT via driver.
' ===================================================================
IMPORT IO
IMPORT SQL
IMPORT RDD
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work

PRINT "=== SQL RDD CRUD ==="
PRINT

' Setup tabella con SQL nativo
SQL OPEN "Data Source=Work/sql_crud.db" ALIAS h
SQL EXEC h, "CREATE TABLE IF NOT EXISTS Emp (ID INT, Name TEXT, Salary REAL)"
SQL EXEC h, "DELETE FROM Emp"
SQL EXEC h, "INSERT INTO Emp VALUES (1, 'Alice', 55000)"
SQL EXEC h, "INSERT INTO Emp VALUES (2, 'Bob', 62000)"
SQL EXEC h, "INSERT INTO Emp VALUES (3, 'Charlie', 48000)"
SQL CLOSE h

' Apri via RDD
RDD USE "Data Source=Work/sql_crud.db" VIA "SQL" ALIAS Emp
PRINT "1. Lettura records"
PRINT "   GETFIELD(1,1)="; RDD.GETFIELD(1, 1)
PRINT "   GETFIELD(1,1)="; RDD.GETFIELD(1, 1)
PRINT "   GETFIELD(1,2)="; RDD.GETFIELD(1, 2)
PRINT

' Lock + buffer
PRINT "2. Lock e sicurezza"
ok = RDD.FLOCK(1)
PRINT "   FLOCK(1)="; ok
RDD UNLOCK 1
ok = RDD.RLOCK(1)
PRINT "   RLOCK(1)="; ok
RDD UNLOCK 1
PRINT

' SETREPROCESS
PRINT "3. SETREPROCESS"
RDD SET REPROCESS 1500
RDD SET REPROCESS 0
PRINT "   OK"
PRINT

' SetOrder (ORDER BY)
PRINT "4. SETORDER"
RDD SET ORDER 1, "Salary"
PRINT "   OK"
PRINT

' CreateIndex
PRINT "5. CREATEINDEX + CLOSEINDEX"
RDD CREATE INDEX 1, "IX_Salary", "Salary"
PRINT "   CREATEINDEX OK"
RDD CLOSE INDEX 1, "IX_Salary"
PRINT "   CLOSEINDEX OK"
PRINT

' Eventi
PRINT "6. Eventi"
RDD ON EVENT "BeforeAppend"
RDD OFF EVENT "BeforeAppend"
PRINT "   OK"
PRINT

' Date/Currency
PRINT "7. Tipi"
d = RDD.TODAY()
PRINT "   TODAY="; d
c = RDD.CURVAL(999.99)
PRINT "   CURVAL="; c
PRINT

KILL g_work + "/sql_crud.db"
PRINT "=== SQL CRUD OK ==="
