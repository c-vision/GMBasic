' SQL minimal CRUD — Phase 0.1 sample
IMPORT SQL WITH ALIAS
IMPORT IO WITH ALIAS
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work

5 KILL g_work + "/sql_minimal.db" : ON ERROR RESUME NEXT
10 SQL OPEN g_work + "/sql_minimal.db" ALIAS AppDb
20 SQL EXEC AppDb, "CREATE TABLE IF NOT EXISTS items (id INTEGER PRIMARY KEY, name TEXT)"
30 SQL EXEC AppDb, "INSERT INTO items (name) VALUES (?)", "widget"
40 SQL EXEC AppDb, "INSERT INTO items (name) VALUES (?)", "gadget"
50 DIM rs AS SQLRESULTSET
60 SQL QUERY AppDb, rs, "SELECT id, name FROM items ORDER BY id"
70 DO WHILE SQL.FETCH(rs)
80     PRINT rs("id"), rs("name")
90 LOOP
100 SQL CLOSE RESULTSET rs
110 SQL CLOSE AppDb
