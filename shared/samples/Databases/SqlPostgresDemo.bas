' SQL PostgreSQL — demo contro PostgreSQL locale (Npgsql)
' ===================================================================
' Apre un database PostgreSQL reale via SQL OPEN con connection string
' Npgsql (Host=...;Database=...;Username=...), crea tabella, inserisce
' con parametri e legge con QUERY/FETCH (stesso pattern dei sample SQLite).
'
' PREREQUISITI: un server PostgreSQL raggiungibile e il database 'gmb_test'
' creato (CREATE DATABASE gmb_test). Il driver SQL rileva 'Host=' nella
' connection string e usa Npgsql automaticamente.
'
' NOTA placeholders: i parametri posizionali (?) sono riscritti dal driver in base
' al provider: $1, $2... per Npgsql, $p0, $p1... per gli altri.
IMPORT SQL WITH ALIAS
IMPORT IO WITH ALIAS
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work

5 KILL g_work + "/sql_pg_check.txt" : ON ERROR RESUME NEXT
10 SQL OPEN "Host=localhost;Port=5432;Database=gmb_test;Username=gaetano" ALIAS Pg
20 SQL EXEC Pg, "DROP TABLE IF EXISTS pg_items"
30 SQL EXEC Pg, "CREATE TABLE pg_items (id SERIAL PRIMARY KEY, name TEXT NOT NULL)"
40 SQL EXEC Pg, "INSERT INTO pg_items (name) VALUES (?)", "widget"
50 SQL EXEC Pg, "INSERT INTO pg_items (name) VALUES (?)", "gadget"
60 SQL EXEC Pg, "INSERT INTO pg_items (name) VALUES (?)", "thing"
70 DIM rs AS SQLRESULTSET
80 SQL QUERY Pg, rs, "SELECT id, name FROM pg_items ORDER BY id"
90 DO WHILE SQL.FETCH(rs)
100     PRINT rs("id"), rs("name")
110 LOOP
120 SQL CLOSE RESULTSET rs

' Funzioni portabili (CHANGES = righe dell'ultimo EXEC; LASTINSERTROWID = ultimo id)
125 DIM lastId AS LONG
130 lastId = SQL.LASTINSERTROWID("Pg")
135 PRINT "LASTINSERTROWID(Pg) = "; lastId
140 SQL EXEC Pg, "UPDATE pg_items SET name = name WHERE id = 2"
145 DIM nChanged AS LONG
150 nChanged = SQL.CHANGES("Pg")
155 PRINT "CHANGES(Pg) = "; nChanged

' Transazioni
160 SQL BEGIN TRANSACTION Pg
170 SQL EXEC Pg, "UPDATE pg_items SET name = 'rolled' WHERE id = 1"
180 SQL ROLLBACK Pg
190 SQL QUERY Pg, rs, "SELECT name FROM pg_items WHERE id = 1"
200 IF SQL.FETCH(rs) THEN PRINT "after rollback:", rs("name")
210 SQL CLOSE RESULTSET rs

220 SQL CLOSE Pg
230 PRINT "--- SQL POSTGRES OK ---"
