' SQL transactions — Phase 0.1 sample
IMPORT SQL WITH ALIAS
IMPORT IO WITH ALIAS
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work

5 KILL g_work + "/sql_tx.db" : ON ERROR RESUME NEXT
10 SQL OPEN g_work + "/sql_tx.db" ALIAS AppDb
20 SQL EXEC AppDb, "CREATE TABLE IF NOT EXISTS accounts (id INTEGER PRIMARY KEY, balance INTEGER)"
30 SQL EXEC AppDb, "INSERT INTO accounts (balance) VALUES (100)"
40 SQL BEGIN TRANSACTION AppDb
50 SQL EXEC AppDb, "UPDATE accounts SET balance = balance - 30 WHERE id = 1"
60 SQL EXEC AppDb, "UPDATE accounts SET balance = balance + 30 WHERE id = 1"
70 SQL ROLLBACK AppDb
80 DIM rs AS SQLRESULTSET
90 SQL QUERY AppDb, rs, "SELECT balance FROM accounts WHERE id = 1"
100 IF SQL.FETCH(rs) THEN
110     PRINT "After rollback:", rs("balance")
120 END IF
130 SQL CLOSE RESULTSET rs
140 SQL BEGIN TRANSACTION AppDb
150 SQL EXEC AppDb, "UPDATE accounts SET balance = 70 WHERE id = 1"
160 SQL COMMIT AppDb
170 SQL QUERY AppDb, rs, "SELECT balance FROM accounts WHERE id = 1"
180 IF SQL.FETCH(rs) THEN
190     PRINT "After commit:", rs("balance")
200 END IF
210 SQL CLOSE RESULTSET rs
220 SQL CLOSE AppDb
