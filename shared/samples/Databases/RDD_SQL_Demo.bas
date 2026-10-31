' RDD_SQL_Demo.bas — SQL RDD con SQLite
' ===================================================================
IMPORT IO
IMPORT SQL
IMPORT RDD
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work

PRINT "=== SQL RDD Demo ==="

' Setup: crea tabella con SQL nativo
SQL OPEN "Data Source=sql_rdd_demo.db" ALIAS h
SQL EXEC h, "CREATE TABLE IF NOT EXISTS Prod (ID INT, Name TEXT, Price REAL)"
SQL EXEC h, "DELETE FROM Prod"
SQL EXEC h, "INSERT INTO Prod VALUES (1, 'Widget', 19.99)"
SQL EXEC h, "INSERT INTO Prod VALUES (2, 'Gadget', 49.99)"
SQL EXEC h, "INSERT INTO Prod VALUES (3, 'Thing', 9.99)"
SQL CLOSE h

' Apri con SQL RDD
RDD USE "Data Source=sql_rdd_demo.db" VIA "SQL" ALIAS Prod

' Leggi campi con SchemaBuffer (il campo Name è una STRINGA → stampa diretta,
' non assegnazione a variabile numerica, altrimenti la conversione lancia)
PRINT "GETFIELD(1,1) ID: "; RDD.GETFIELD(1, 1)
PRINT "GETFIELD(1,1) Name: "; RDD.GETFIELD(1, 1)
PRINT "GETFIELD(1,2) Price: "; RDD.GETFIELD(1, 2)

' Lock
ok = RDD.RLOCK(1)
PRINT "RLOCK(1): "; ok
RDD UNLOCK 1

' Date
d = RDD.TODAY()
PRINT "TODAY: "; d
c = RDD.CURVAL(99.99)
PRINT "CURVAL: "; c

' SETREPROCESS
RDD SET REPROCESS 1500
RDD SET REPROCESS 0

' Eventi
RDD ON EVENT "BeforeNavigate"
RDD OFF EVENT "BeforeNavigate"

KILL g_work + "/sql_rdd_demo.db"
PRINT "=== SQL RDD OK ==="
