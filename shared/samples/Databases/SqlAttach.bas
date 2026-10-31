' SQL ATTACH + cross-schema query — Phase 0 sample
IMPORT SQL WITH ALIAS
IMPORT IO WITH ALIAS
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work

10 KILL g_work + "/sql_attach_main.db" : ON ERROR RESUME NEXT
20 KILL g_work + "/sql_attach_aux.db" : ON ERROR RESUME NEXT
30 SQL OPEN g_work + "/sql_attach_main.db" ALIAS MainDb
40 SQL EXEC MainDb, "CREATE TABLE main_items (id INTEGER PRIMARY KEY, label TEXT)"
50 SQL EXEC MainDb, "INSERT INTO main_items (label) VALUES ('main')"
60 SQL OPEN g_work + "/sql_attach_aux.db" ALIAS AuxFile
70 SQL EXEC AuxFile, "CREATE TABLE aux_items (id INTEGER PRIMARY KEY, label TEXT)"
80 SQL EXEC AuxFile, "INSERT INTO aux_items (label) VALUES ('aux')"
90 SQL CLOSE AuxFile
100 SQL ATTACH MainDb, g_work + "/sql_attach_aux.db" AS aux
110 DIM rs AS SQLRESULTSET
120 SQL QUERY MainDb, rs, "SELECT label FROM aux.aux_items"
130 IF SQL.FETCH(rs) THEN
140     PRINT "aux label:", rs("label")
150 END IF
160 SQL CLOSE RESULTSET rs
170 SQL QUERY MainDb, rs, "SELECT label FROM main_items"
180 IF SQL.FETCH(rs) THEN
190     PRINT "main label:", rs("label")
200 END IF
210 SQL CLOSE RESULTSET rs
220 SQL DETACH MainDb, "aux"
230 SQL CLOSE MainDb
