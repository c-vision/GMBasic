' SQL — funzionalità avanzate (superficie potenziata)
' ===================================================================
' Dimostra le aggiunte alla superficie SQL:
'   1. Result set MATERIALIZZATO: rs(i) per indice, rs.COUNT, rs.COLUMNS
'   2. SQL BACKUP (SQLite): copia fisica del database; su altri provider → errore
'   3. Prepared statement (riuso piani) + parametri tipizzati (transparenti)
'   4. Savepoint: SQL SAVEPOINT / RELEASE / ROLLBACK TO SAVEPOINT
'   5. SQL.LASTINSERTROWID() / SQL.CHANGES() portabili
'
' Richiede: SQLite (nessun server esterno).
IMPORT SQL WITH ALIAS
IMPORT IO WITH ALIAS
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work

5 KILL g_work + "/adv.db" : ON ERROR RESUME NEXT
6 KILL g_work + "/adv_backup.db" : ON ERROR RESUME NEXT

10 SQL OPEN g_work + "/adv.db" ALIAS AppDb
20 SQL EXEC AppDb, "CREATE TABLE items (id INTEGER PRIMARY KEY, name TEXT, price REAL)"
30 SQL EXEC AppDb, "INSERT INTO items (name, price) VALUES (?, ?)", "widget", 19.99
40 SQL EXEC AppDb, "INSERT INTO items (name, price) VALUES (?, ?)", "gadget", 49.99
50 SQL EXEC AppDb, "INSERT INTO items (name, price) VALUES (?, ?)", "thing", 9.99

' 1. Result set materializzato
60 DIM rs AS SQLRESULTSET
70 SQL QUERY AppDb, rs, "SELECT id, name, price FROM items ORDER BY id"
80 PRINT "COUNT="; rs.COUNT
90 PRINT "COLUMNS="; rs.COLUMNS
100 DO WHILE SQL.FETCH(rs)
110     PRINT rs("id"); "/"; rs(1); "/"; rs(2)
120 LOOP
130 SQL CLOSE RESULTSET rs

' 2. Backup SQLite (copie reali, rileggibili)
140 SQL BACKUP AppDb TO g_work + "/adv_backup.db"
150 SQL OPEN g_work + "/adv_backup.db" ALIAS AppBk
160 SQL QUERY AppBk, rs, "SELECT COUNT(*) AS n FROM items"
170 IF SQL.FETCH(rs) THEN PRINT "backup rows="; rs("n")
180 SQL CLOSE RESULTSET rs
190 SQL CLOSE AppBk

' 3-4. Transazione + savepoint (solo la riga 'keep' deve sopravvivere)
200 SQL BEGIN TRANSACTION AppDb
210 SQL SAVEPOINT AppDb, "sp1"
220 SQL EXEC AppDb, "INSERT INTO items (name, price) VALUES ('keep', 1.00)"
230 SQL SAVEPOINT AppDb, "sp2"
240 SQL EXEC AppDb, "INSERT INTO items (name, price) VALUES ('drop', 2.00)"
250 SQL ROLLBACK TO SAVEPOINT AppDb, "sp2"
260 SQL RELEASE SAVEPOINT AppDb, "sp1"
270 SQL COMMIT AppDb

' 5. Funzioni portabili
280 DIM lastId AS LONG
290 lastId = SQL.LASTINSERTROWID("AppDb")
300 PRINT "LASTINSERTROWID="; lastId
310 SQL EXEC AppDb, "UPDATE items SET price = price WHERE id = 1"
320 DIM nChanged AS LONG
330 nChanged = SQL.CHANGES("AppDb")
340 PRINT "CHANGES="; nChanged

' Verifica finale
350 SQL QUERY AppDb, rs, "SELECT name FROM items WHERE name IN ('keep', 'drop') ORDER BY name"
360 DO WHILE SQL.FETCH(rs)
370     PRINT "tx result: "; rs("name")
380 LOOP
390 SQL CLOSE RESULTSET rs

400 SQL CLOSE AppDb
410 PRINT "--- SQL ADVANCED OK ---"
