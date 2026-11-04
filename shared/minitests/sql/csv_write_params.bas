' CSV scrivibile: parametri (delim/header), quoting (delim/punto-e-virgola), delete-all, reinsert.
' EXPECT: row  1  [alice] [ok]|row  2  [a,b] [x;y]|after-del= 0|reinsert= 1
IMPORT SQL
IMPORT IO

SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
    MKDIR GMBWORK$
    ON ERROR RESUME NEXT
    IF IO.FILEEXISTS(GMBWORK$ + "/p.csv") THEN KILL GMBWORK$ + "/p.csv"
    ON ERROR GOTO 0
    OPEN GMBWORK$ + "/p.csv" FOR OUTPUT AS #1
    PRINT #1, "id;name;note"
    PRINT #1, "1;alice;ok"
    CLOSE #1
    SQL OPEN ":memory:" ALIAS M
    SQL EXEC M, "CREATE VIRTUAL TABLE t USING csv(name='work/p.csv', delim=';')"
    ' valore con virgola e con punto-e-virgola (delim) → deve essere quotato in scrittura e riletto intero
    SQL EXEC M, "INSERT INTO t VALUES (2,'a,b','x;y')"
    DIM rs AS SQLRESULTSET
    SQL QUERY M, rs, "SELECT id, name, note FROM t ORDER BY id"
    DO WHILE SQL.FETCH(rs)
        PRINT "row "; rs("id"); " ["; rs("name"); "] ["; rs("note"); "]"
    LOOP
    SQL CLOSE RESULTSET rs
    SQL EXEC M, "DELETE FROM t"
    SQL QUERY M, rs, "SELECT COUNT(*) AS n FROM t"
    IF SQL.FETCH(rs) THEN
        PRINT "after-del="; rs("n")
    END IF
    SQL CLOSE RESULTSET rs
    SQL EXEC M, "INSERT INTO t VALUES (9,'z','w')"
    SQL QUERY M, rs, "SELECT COUNT(*) AS n FROM t"
    IF SQL.FETCH(rs) THEN
        PRINT "reinsert="; rs("n")
    END IF
    SQL CLOSE RESULTSET rs
    SQL CLOSE M
END SUB
